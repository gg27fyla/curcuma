/*
 * <Curcuma main file.>
 * Copyright (C) 2019 - 2022 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "src/core/fileiterator.h"
#include "src/core/molecule.h"
#include "src/core/uff.h"

#include "src/capabilities/analysenciplot.h"
#include "src/capabilities/confscan.h"
#include "src/capabilities/confsearch.h"
#include "src/capabilities/confstat.h"
#include "src/capabilities/curcumaopt.h"
#include "src/capabilities/docking.h"
#include "src/capabilities/nebdocking.h"
#include "src/capabilities/pairmapper.h"
#include "src/capabilities/persistentdiagram.h"
#include "src/capabilities/rmsd.h"
#include "src/capabilities/rmsdtraj.h"
#include "src/capabilities/simplemd.h"

#include "src/tools/general.h"
#include "src/tools/info.h"

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <vector>

#ifdef C17
#ifndef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif

// #include "ripser.h"

#ifndef _WIN32
#if __GNUC__
// Thanks to
// https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void bt_handler(int sig)
{
    void* array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Curcuma crashed. Although this is probably unintended, it happened anyway.\n Some kind of backtrace will be printed out!\n\n");
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "Good-By\n");
#ifdef C17
#ifndef _WIN32
    //  std::filesystem::remove("stop");
#endif
#else
    remove("stop");
#endif
    exit(1);
}

void ctrl_c_handler(int s)
{
    if (std::filesystem::exists("stop")) {
#ifdef C17
#ifndef _WIN32
        std::filesystem::remove("stop");
#endif
#else
        remove("stop");
#endif
        printf("Caught stop signal a second time.\nWill exit now!\n\n");
        exit(1);
    } else {
        printf("Caught stop signal\nWill try to stop current stuff!\n");
        std::ofstream output("stop");
    }
}

#endif
#endif

#include "json.hpp"
using json = nlohmann::json;

void Distance(const Molecule &mol, char **argv)
{
    int donor = stoi(std::string(argv[3]));
    int proton = stoi(std::string(argv[4]));
    int acceptor = stoi(std::string(argv[5]));
    std::cout << "Using atoms " << donor << " " << proton << " " << acceptor << std::endl;
    std::cout << "Donor ";
    mol.printAtom(donor);
    std::cout << std::endl
              << "Proton: ";
    mol.printAtom(proton);
    std::cout << std::endl
              << "Acceptor: ";
    mol.printAtom(acceptor);
    std::cout << std::endl
              << "Hydrogen Bond Angle: " << mol.CalculateAngle(donor - 1, proton - 1, acceptor - 1) << std::endl;
    std::cout << "Hydrogen bond length " << mol.CalculateDistance(proton - 1, acceptor - 1) << std::endl;
}

int main(int argc, char **argv) {
#ifndef _WIN32
#if __GNUC__
    signal(SIGINT, ctrl_c_handler);
    signal(SIGSEGV, bt_handler);
    signal(SIGABRT, bt_handler);
#endif
#endif

    General::StartUp(argc, argv);
#ifdef C17
#ifndef _WIN32
    std::filesystem::remove("stop");
#endif
#else
    remove("stop");
#endif
    RunTimer timer(true);
    if(argc < 2)
    {
        std::cerr << "No arguments given!" << std::endl;
        std::cerr << "Use:" << std::endl
                  << "-rmsd        * RMSD Calculator                                            *" << std::endl
                  << "-confscan    * Filter list of conformers                                  *" << std::endl
                  << "-confstat    * Conformation statistics                                    *" << std::endl
                  << "-dock        * Perform some docking                                       *" << std::endl;
        std::cout << "-opt         * LBFGS optimiser using xtb GFN                              *" << std::endl;
        std::cout << "-sp          * Single point calculation using xtb GFN                     *" << std::endl;
        std::cout << "-md          * Molecular dynamics using xtb GFN                           *" << std::endl;
        std::cout << "-block       * Split files with many structures in block                  *" << std::endl
                  << "-distance    * Calculate distance between two atoms                       *" << std::endl
                  << "-angle       * Calculate angle between three atoms                        *" << std::endl
                  << "-split       * Split a supramolcular structure in individual molecules    *" << std::endl
                  << "-rmsdtraj    * Find unique structures                                     *" << std::endl
                  << "-distance    * Calculate distance matrix                                  *" << std::endl
                  << "-reorder     * Write molecule file with randomly reordered indices        *" << std::endl;
        exit(1);
    }
    if(argc >= 2)
    {
        json controller = CLI2Json(argc, argv);

        if(strcmp(argv[1], "-rmsd") == 0)
        {
            if (argc < 4) {
                std::cerr << "Please use curcuma for rmsd calcultion as follows\ncurcuma -rmsd A.xyz B.xyz" << std::endl;
                std::cerr << "Additonal arguments are:" << std::endl;
                std::cerr << "-reorder    **** Force reordering of structure!" << std::endl;
                std::cerr << "-check      **** Check methyl group connectivity." << std::endl;
                std::cerr << "-heavy      **** Calculate RMSD for heavy atoms only. Affects Reordering." << std::endl;
                std::cerr << "-fragment n **** Use n'th fragment. Bonds are determined from simple covalent radii for now!" << std::endl;
                std::cerr << "-init n     **** Initialse Reordering with fixed fragement n" << std::endl;
                exit(1);
            }

            Molecule mol1(argv[2]); // will only take first structure
            Molecule mol2(argv[3]); // will only take first structure

            if (mol1.AtomCount() == 0 || mol2.AtomCount() == 0) {
                std::cout << "At least one structure is empty:\n";
                std::cout << argv[2] << " " << mol1.AtomCount() << " atoms" << std::endl;
                std::cout << argv[3] << " " << mol2.AtomCount() << " atoms" << std::endl;
                exit(0);
            }

            std::string reffile = argv[2];
            std::string tarfile = argv[3];
            reffile.erase(reffile.end() - 4, reffile.end());
            tarfile.erase(tarfile.end() - 4, tarfile.end());
            RMSDDriver* driver = new RMSDDriver(controller, false);
            driver->setReference(mol1);
            driver->setTarget(mol2);
            driver->start();
            std::cout << "RMSD for two molecules " << driver->RMSD() << std::endl;

            driver->ReferenceAligned().writeXYZFile(reffile + "_centered.xyz");
            driver->TargetAligned().writeXYZFile(tarfile + "_centered.xyz");
            driver->TargetReorderd().writeXYZFile(tarfile + "_reordered.xyz");

            std::cout << Tools::Vector2String(driver->ReorderRules()) << std::endl;
            delete driver;
            exit(0);
        } else if (strcmp(argv[1], "-dock") == 0) {
            if (argc < 4) {
                std::cerr << "Please use curcuma for docking as follows\ncurcuma -dock -host A.xyz -guest B.xyz -Step_x 10 -Step_y 10 -Step_z 10" << std::endl;
                exit(1);
            }

            Docking* docking = new Docking(controller, false);
            if (docking->Initialise() == false) {
                docking->printError();
                return 0;
            }
            docking->start();
        } else if (strcmp(argv[1], "-hbonds") == 0) {
            if(argc != 6)
            {
                std::cerr << "Please use curcuma for hydrogen bond analysis as follows\ncurcuma -hbonds A.xyz index_donor index_proton index_acceptor" << std::endl;
                return -1;
            }
            FileIterator file(argv[2]);
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                if (argc == 6) {
                    if (std::string(argv[1]).find("-hbonds") != std::string::npos) {
                        Distance(mol, argv);
                    }
                } else {
                    mol.print_geom();
                    std::cout << std::endl
                              << std::endl;
                    std::cout << mol.getGeometry() << std::endl;
                }
            }
        } else if (strcmp(argv[1], "-confscan") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma for conformation scan and judge as follows\ncurcuma -confscan conffile.xyz" << std::endl;
                std::cerr << "Additonal arguments are:" << std::endl;
                std::cerr << "-writeXYZ  **** Write results to xyz files!" << std::endl;
                std::cerr << "-rank n    **** Write only the first n results!" << std::endl;
                std::cerr << "-reorder   **** Force reordering of structure! - It will be done automatically, if energies are close and rmsd is big." << std::endl;
                std::cerr << "-heavy     **** Use only heavy atoms for rmsd calculation." << std::endl;
                std::cerr << "-noname    **** Do not read possible name from xyz file." << std::endl;
                std::cerr << "-noreorder **** Prevent reordering in any cases." << std::endl;
                std::cerr << "-norestart **** Prevent restarting in any cases." << std::endl;
                std::cerr << "-maxenergy **** Maximal energy difference between best and current conformer [kJ/mol] for a conformer to be analysed." << std::endl;
                std::cerr << "-energy    **** Energy threshold for identical structures [kJ/mol]." << std::endl;
                return -1;
            }
            std::cout << controller << std::endl;
            ConfScan* scan = new ConfScan(controller);
            scan->setFileName(argv[2]);
            scan->start();
            return 0;
        } else if (strcmp(argv[1], "-confstat") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma for conformation statistics as follows\ncurcuma -confstat conffile.xyz" << std::endl;

                return -1;
            }
            ConfStat* stat = new ConfStat(controller);
            stat->setFileName(argv[2]);
            stat->start();
            return 0;
        } else if (strcmp(argv[1], "-led") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for fragment assignment as follows:\ncurcuma -led input.xyz" << std::endl;
                return 0;
            }

            Molecule mol1 = Files::LoadFile(argv[2]);
            if (mol1.Atoms().size())
                mol1.printFragmente();
        } else if (strcmp(argv[1], "-hmap") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for hydrogen bond mapping as follows:\ncurcuma -hmap trajectory.xyz" << std::endl;
                return 0;
            }

            std::vector<std::pair<int, int>> pairs, elements;

            if (argc >= 3) {
                for (std::size_t i = 3; i < argc; ++i) {
                    if (strcmp(argv[i], "-pair") == 0) {
                        if (i + 2 < argc) {
                            if (Tools::isInt(argv[i + 1]) && Tools::isInt(argv[i + 2])) {
                                int first = std::stoi(argv[i + 1]) - 1;
                                int second = std::stoi(argv[i + 2]) - 1;
                                ++i;
                                pairs.push_back(std::pair<int, int>(first, second));
                            } else {
                                int first = Elements::String2Element(argv[i + 1]);
                                int second = Elements::String2Element(argv[i + 2]);
                                ++i;
                                elements.push_back(std::pair<int, int>(first, second));
                            }
                        }
                    }
                    if (strcmp(argv[i], "-pairfile") == 0) {
                        if (i + 1 < argc) {
                            std::ifstream input(argv[i + 1]);
                            for (std::string line; getline(input, line);) {
                                std::vector<std::string> numbers = Tools::SplitString(line);
                                if (numbers.size() == 2) {
                                    if (Tools::isInt(numbers[0]) && Tools::isInt(numbers[1])) {
                                        int first = std::stoi(numbers[0]) - 1;
                                        int second = std::stoi(numbers[1]) - 1;
                                        pairs.push_back(std::pair<int, int>(first, second));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PairMapper mapper;
            mapper.setFile(argv[2]);
            for (const std::pair<int, int>& pair : pairs)
                mapper.addPair(pair);
            for (const std::pair<int, int>& pair : elements)
                mapper.addElementPair(pair);

            mapper.FindPairs();
        } else if (strcmp(argv[1], "-nci") == 0) {

            if (argc < 4) {
                std::cerr << "Please use curcuma to post-process two RDG vs rho plots from NCIPLOT as follows:\ncurcuma -nci file1.dat file2.dat" << std::endl;
                std::cerr << "Additonal arguments are:" << std::endl;
                std::cerr << "-bins             **** Number of bins during indexing the file!" << std::endl;
                std::cerr << "-scale_d1         **** Scale minimal distance for file1.dat!" << std::endl;
                std::cerr << "-scale_d2         **** Scale minimal distance for file2.dat!" << std::endl;
                std::cerr << "-local_distance   **** Recalculate distance for every bin (false = default)" << std::endl;
                return 0;
            }

            AnalyseNCIPlot analyse(controller);
            analyse.setFiles(argv[2], argv[3]);
            analyse.start();

        } else if (strcmp(argv[1], "-opt") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for optimisation as follows:\ncurcuma -opt input.xyz" << std::endl;
                return 0;
            }
            CurcumaOpt opt(controller, false);
            opt.setFileName(argv[2]);
            opt.start();
            return 0;
        } else if (strcmp(argv[1], "-sp") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for energy calculation as follows:\ncurcuma -opt input.xyz" << std::endl;
                return 0;
            }
            json sp = controller["sp"];
            sp["SinglePoint"] = true;
            controller["sp"] = sp;
            CurcumaOpt opt(controller, false);
            opt.setFileName(argv[2]);
            opt.start();
            return 0;
        } else if (strcmp(argv[1], "-block") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma to split a file with many structures (trajectories) into several smaller:\ncurcuma block input.xyz X" << std::endl;
                std::cerr << "With X the number of files to produce!" << std::endl;

                return 0;
            }
            int blocks = std::stoi(argv[3]);
            std::string outfile = std::string(argv[2]);
            for (int i = 0; i < 4; ++i)
                outfile.pop_back();
            FileIterator file(argv[2]);
            int mols = file.MaxMolecules();
            std::multimap<double, Molecule> results;
            int block = mols / blocks;
            int index = 0;
            int i = 0;
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                mol.appendXYZFile(outfile + "_" + std::to_string(index + 1) + ".xyz");
                i++;
                if (i > block) {
                    index++;
                    i = 0;
                }
            }

            return 0;
        } else if (strcmp(argv[1], "-md") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for molecular dynamics simulation as follows:\ncurcuma -md input.xyz" << std::endl;
                return 0;
            }

            Molecule mol1 = Files::LoadFile(argv[2]);
            SimpleMD md(controller, false);
            md.setMolecule(mol1);
            std::string name = std::string(argv[2]);
            for (int i = 0; i < 4; ++i)
                name.pop_back();
            md.setBaseName(name);

            md.Initialise();
            md.start();
        } else if (strcmp(argv[1], "-confsearch") == 0) {
            if (argc < 2) {
                std::cerr << "Please use curcuma for conformational search as follows:\ncurcuma -confsearch input.xyz" << std::endl;
                return 0;
            }

            ConfSearch confsearch(controller, false);
            confsearch.setFile(argv[2]);
            confsearch.start();
        } else if (strcmp(argv[1], "-rmsdtraj") == 0) {
            if (argc <= 2) {
                std::cerr << "Please use curcuma for rmsd analysis of trajectories as follows:\ncurcuma -rmsdtraj input.xyz" << std::endl;
                std::cerr << "Additonal arguments are:" << std::endl;
                std::cerr << "-write        **** Write unique conformers!" << std::endl;
                std::cerr << "-rmsd d       **** Set rmsd threshold to d ( default = 1.0)!" << std::endl;
                std::cerr << "-fragment n   **** Set fragment to n." << std::endl;
                std::cerr << "-reference    **** Add different xyz structure as reference." << std::endl;
                std::cerr << "-second       **** Add second trajectory." << std::endl;
                std::cerr << "-heavy        **** Check only heavy atoms. Do not use with -write." << std::endl;
                return 0;
            }

            RMSDTraj traj(controller, false);
            traj.setFile(argv[2]);
            traj.Initialise();
            traj.start();

        } else if (strcmp(argv[1], "-nebprep") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma for geometry preparation for nudge-elastic-band calculation follows:\ncurcuma -nebprep first.xyz second.xyz" << std::endl;
                return 0;
            }
            int pt = 0;
            for (std::size_t i = 3; i < argc; ++i) {
                if (strcmp(argv[i], "-pt") == 0) {
                    if (i + 1 < argc) {
                        pt = std::stoi(argv[i + 1]);
                        ++i;
                    }
                }
            }

            Molecule mol1 = Files::LoadFile(argv[2]);
            Molecule mol2 = Files::LoadFile(argv[3]);
            NEBDocking* nebdock = new NEBDocking;
            nebdock->setStructures(mol1, mol2);
            nebdock->setProtonTransfer(pt);
            nebdock->Prepare();
            delete nebdock;
        } else if (strcmp(argv[1], "-centroid") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma for centroid calculation of user definable fragments:\ncurcuma -centroid first.xyz" << std::endl;
                return 0;
            }

            int pt = 0, fragment = 0;
            std::vector<int> frag;
            for (std::size_t i = 3; i < argc; ++i) {
                if (strcmp(argv[i], "-fragment") == 0) {
                    if (i + 1 < argc) {
                        fragment = std::stoi(argv[i + 1]);
                        ++i;
                    }
                    // continue;
                }
                if (strcmp(argv[i], "-addfragment") == 0) {
                    bool loop = true;
                    while (i + 1 < argc && loop) {
                        StringList list = Tools::SplitString(argv[i + 1]);
                        if (list.size() > 1) {
                            for (auto element : list)
                                frag.push_back(std::stoi(element) - 1);
                        }
                        if (Tools::isInt(argv[i + 1]))
                            frag.push_back(std::stoi(argv[i + 1]) - 1);
                        else
                            loop = false;
                        ++i;
                    }
                    // continue;
                }
            }

            if (frag.size()) {
                std::cout << "Using fragment of atoms :";
                for (int atom : frag)
                    std::cout << atom + 1 << " ";
                std::cout << std::endl;
                std::cout << "to calculate centroid!" << std::endl;
            }

            std::ofstream result_file;
            result_file.open("centroids.dat");
            FileIterator file(argv[2]);
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                if (frag.size()) {
                    result_file << GeometryTools::Centroid(mol.getGeometry(frag)).transpose() << std::endl;
                    std::cout << mol.getGeometry(frag) << std::endl;
                } else {
                    mol.GetFragments(1.2);
                    result_file << GeometryTools::Centroid(mol.getGeometryByFragment(fragment)).transpose() << std::endl;
                }
            }
        } else if (strcmp(argv[1], "-split") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma to split supramolecular structures as follows:\ncurcuma -split molecule.xyz" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);
            int index = 1;
            std::string outfile = argv[2];
            for (int i = 0; i < 4; ++i)
                outfile.pop_back();
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                mol.setScaling(1.2);
                std::cout << file.MaxMolecules() << std::endl;
                if (file.MaxMolecules() <= 1)
                    mol.writeXYZFragments(outfile);
                else
                    mol.writeXYZFragments(outfile + "_M" + std::to_string(index));
                index++;
            }

        } else if (strcmp(argv[1], "-distance") == 0) {
            if (argc < 4) {
                std::cerr << "Please use curcuma to calculate distances as follows:\ncurcuma -distance molecule.xyz indexA indexB" << std::endl;
                return 0;
            }

            int indexA = 0, indexB = 0;
            try {
                indexA = std::stoi(argv[3]);
            } catch (const std::invalid_argument& arg) {
                std::cerr << "Please use curcuma to calculate distances as follows:\ncurcuma -distance molecule.xyz indexA indexB" << std::endl;
                return 0;
            }
            try {
                indexB = std::stoi(argv[4]);
            } catch (const std::invalid_argument& arg) {
                std::cerr << "Please use curcuma to calculate distances as follows:\ncurcuma -distance molecule.xyz indexA indexB" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);
            std::string outfile = argv[2];

            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                std::cout << ":: " << mol.CalculateDistance(indexA - 1, indexB - 1) << "::" << std::endl;
            }

        } else if (strcmp(argv[1], "-angle") == 0) {
            if (argc < 6) {
                std::cerr << "Please use curcuma to calculate angles as follows:\ncurcuma -angle molecule.xyz indexA indexB indexC" << std::endl;
                return 0;
            }

            int indexA = 0, indexB = 0, indexC = 0;
            try {
                indexA = std::stoi(argv[3]);
            } catch (const std::invalid_argument& arg) {
                std::cerr << "Please use curcuma to calculate angles as follows:\ncurcuma -angle molecule.xyz indexA indexB indexC" << std::endl;
                return 0;
            }
            try {
                indexB = std::stoi(argv[4]);
            } catch (const std::invalid_argument& arg) {
                std::cerr << "Please use curcuma to calculate angles as follows:\ncurcuma -angle molecule.xyz indexA indexB indexC" << std::endl;
                return 0;
            }
            try {
                indexC = std::stoi(argv[5]);
            } catch (const std::invalid_argument& arg) {
                std::cerr << "Please use curcuma to calculate angles as follows:\ncurcuma -angle molecule.xyz indexA indexB indexC" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);

            printf("\n  Angle\t\tr(%u,%u)\tr(%u,%u)\tr(%u,%u)\n", indexA - 1, indexB - 1, indexA - 1, indexC - 1, indexC - 1, indexB - 1);

            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                printf(":: %8.4f\t%8.4f\t%8.4f\t%8.4f ::\n", mol.CalculateAngle(indexA - 1, indexB - 1, indexC - 1), mol.CalculateDistance(indexA - 1, indexB - 1), mol.CalculateDistance(indexA - 1, indexC - 1), mol.CalculateDistance(indexC - 1, indexB - 1));
            }
            printf("\n\n");
        } else if (strcmp(argv[1], "-dMatrix") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma to calculate a distance matrix for a molecule as follows:\ncurcuma -dMatrix molecule.xyz" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);
            bool print = false;
            json dMatrix = controller["dMatrix"];
            std::string outfile;
            try {
                outfile = Json2KeyWord<std::string>(dMatrix, "o");
            } catch (int i) {
                if (i == -1) {
                    print = true;
                }
            }
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                if (print) {
                    std::cout << mol.LowerDistanceMatrix();

                    auto vector = mol.LowerDistanceVector();
                    /*
                    index_t dim_max = 2;
                    value_t threshold = std::numeric_limits<value_t>::max();
                    float ratio = 1;
                    coefficient_t modulus = 2;
                    compressed_lower_distance_matrix dist = compressed_lower_distance_matrix(std::move(vector));

                    value_t enclosing_radius = std::numeric_limits<value_t>::infinity();
                    if (threshold == std::numeric_limits<value_t>::max()) {
                        for (size_t i = 0; i < dist.size(); ++i) {
                            value_t r_i = -std::numeric_limits<value_t>::infinity();
                            for (size_t j = 0; j < dist.size(); ++j)
                                r_i = std::max(r_i, dist(i, j));
                            enclosing_radius = std::min(enclosing_radius, r_i);
                        }
                    }

                    auto result = ripser<compressed_lower_distance_matrix>(std::move(dist), dim_max, enclosing_radius,
                        ratio, modulus)
                                      .compute_barcodes();

                    for(const auto &r : result)
                    {
                        for(const auto &v : r.second)
                            std::cout << v.first << " " << v.second << std::endl;

                    }
  */
                    PersistentDiagram diagram;
                    diagram.setDistanceMatrix(vector);
                    auto l = diagram.generatePairs();

                    for (const auto& r : l) {
                        std::cout << r.first << " " << r.second << std::endl;
                    }
                    diagram.setXRange(0, 4);
                    diagram.setYRange(0, 4);
                    std::cout << diagram.generateImage(l);
                } else {
                    std::ofstream input;
                    input.open(outfile, std::ios::out);
                    input << mol.LowerDistanceMatrix();
                    input.close();
                }
            }
        } else if (strcmp(argv[1], "-center") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma to center a structure as follows:\ncurcuma -center molecule.xyz" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);
            int index = 1;
            std::string outfile = argv[2];
            for (int i = 0; i < 4; ++i)
                outfile.pop_back();
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                std::cout << mol.Centroid() << std::endl;
                // mol.setGeometry(GeometryTools::TranslateGeometry(mol.getGeometry(), GeometryTools::Centroid(mol.getGeometry()), Position{ 0, 0, 0 }));
                mol.Center();
                std::cout << mol.Centroid() << std::endl;

                if (file.MaxMolecules() <= 1)
                    mol.writeXYZFragments(outfile);
                else
                    mol.writeXYZFragments(outfile + "_M" + std::to_string(index));
                index++;
            }
        } else if (strcmp(argv[1], "-reorder") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma to center a structure as follows:\ncurcuma -center molecule.xyz" << std::endl;
                return 0;
            }
            FileIterator file(argv[2]);
            int index = 1;
            std::string outfile = argv[2];
            for (int i = 0; i < 4; ++i)
                outfile.pop_back();
            outfile += ".random.xyz";
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                mol.writeXYZFile(outfile, Tools::RandomVector(0, mol.AtomCount()));
            }
        } else {
            bool centered = false;
            for (std::size_t i = 2; i < argc; ++i) {
                if (strcmp(argv[i], "-center") == 0) {
                    centered = true;
                }
            }

            FileIterator file(argv[1]);
            while (!file.AtEnd()) {
                Molecule mol = file.Next();
                mol.setScaling(1.2);
                mol.CalculateRotationalConstants();
                if (centered)
                    mol.setGeometry(GeometryTools::TranslateGeometry(mol.getGeometry(), GeometryTools::Centroid(mol.getGeometry()), Position{ 0, 0, 0 }));
                mol.print_geom();
                mol.AnalyseIntermoleculeDistance();
                std::cout << mol.Check() << std::endl
                          << std::endl;

                                UFF forcefield;
                                forcefield.setMolecule(mol.Atoms(), mol.Coords());
                                // forcefield.readParameterFile("parameter.json");

                                forcefield.Initialise();
                                std::cout << forcefield.Calculate(true) << std::endl;
                                forcefield.writeParameterFile("parameter.json");
                                forcefield.readParameterFile("parameter.json");
                                std::cout << forcefield.Calculate(true) << std::endl;

                                /*        double grad[3 * mol.AtomCount()];
                                          forcefield.Gradient(grad);
                                          forcefield.NumGrad(grad);
                                          */
                                // CompactTopo(mol.HydrogenBondMatrix(-1,-1));
                                // std::cout << CompareTopoMatrix(mol.HydrogenBondMatrix(-1,-1),mol.HydrogenBondMatrix(-1,-1) ) << std::endl;
                                // auto m = mol.DistanceMatrix();
                                // std::cout << m.first << std::endl;
                                // std::cout << m.second << std::endl;
            }
        }
    }
#ifdef C17
#ifndef _WIN32
    std::filesystem::remove("stop");
#endif
#else
    remove("stop");
#endif

    return 0;
}
