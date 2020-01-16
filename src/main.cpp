/*
 * <Internal Coordinate Handler for chemical structures.>
 * Copyright (C) 2019 - 2020 Conrad Hübler <Conrad.Huebler@gmx.net>
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

#include "src/core/molecule.h"

#include "src/capabilities/confscan.h"
#include "src/capabilities/docking.h"
#include "src/capabilities/rmsd.h"

#include "src/tools/general.h"

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <vector>


void Distance(const Molecule &mol, char **argv)
{
    int donor = stoi(std::string(argv[3]));
    int proton = stoi(std::string(argv[4]));
    int acceptor = stoi(std::string(argv[5]));
    std::cout << "Using atoms" << std::endl;
    std::cout << "Donor ";
    mol.printAtom(donor);
    std::cout << endl << "Proton: ";
    mol.printAtom(proton);
    std::cout << endl << "Acceptor: ";
    mol.printAtom(acceptor);
    std::cout << std::endl << "Hydrogen Bond Angle: "<<  mol.angle(donor, proton, acceptor) << std::endl;
    std::cout << "Hydrogen bond length " << mol.Distance(proton - 1, acceptor - 1) << std::endl;

}



int main(int argc, char **argv) {
    
    
    if(argc < 2)
    {
        std::cerr << "No arguments given!" << std::endl;
    }
    
    if(argc >= 2)
    {

        if(strcmp(argv[1], "-rmsd") == 0)
        {
            if (argc < 4) {
                std::cerr << "Please use curcuma for rmsd calcultion as follows\ncurcuma -rmsd A.xyz B.xyz" << std::endl;
                exit(1);
            }
            Molecule mol1 = Tools::LoadFile(argv[2]);
            Molecule mol2 = Tools::LoadFile(argv[3]);

            bool reorder = false, check_connect = false;
            int fragment = -1, pt = 0;
            if (argc >= 5) {

                for (std::size_t i = 4; i < argc; ++i) {
                    // std::cout << argv[i] << " " << strcmp(argv[i], "-fragment") << std::endl;
                    if (strcmp(argv[i], "-fragment") == 0) {
                        if (i + 1 < argc) {
                            fragment = std::stoi(argv[i + 1]);
                            ++i;
                        }
                        ++i;
                        continue;
                    }

                    if (strcmp(argv[i], "-pt") == 0) {
                        if (i + 1 < argc) {
                            pt = std::stoi(argv[i + 1]);
                            ++i;
                        }
                        ++i;
                        continue;
                    }

                    if (strcmp(argv[i], "-reorder") == 0) {
                        reorder = true;
                        ++i;
                        continue;
                    }

                    if (strcmp(argv[i], "-check") == 0) {
                        check_connect = true;
                        ++i;
                        continue;
                    }
                }
        }

        mol1.print_geom();
        mol2.print_geom();

        RMSDDriver *driver = new RMSDDriver(mol1, mol2);
        driver->setForceReorder(reorder);
        driver->setFragment(fragment);
        driver->setCheckConnections(check_connect);
        driver->AutoPilot();
        std::cout << "RMSD for two molecules " << driver->RMSD() << std::endl;

        driver->ReferenceAligned().writeXYZFile("reference.xyz");
        driver->TargetAligned().writeXYZFile("target_align.xyz");
        driver->TargetReorderd().writeXYZFile("target_reorder.xyz");

        delete driver;

        exit(0);
        } else if (strcmp(argv[1], "-dock") == 0) {

            if (argc < 4) {
                std::cerr << "Please use curcuma for rmsd calcultion as follows\ncurcuma -dock A.xyz B.xyz XXX YYY ZZZ" << std::endl;
                exit(1);
            }
            Molecule mol1 = Tools::LoadFile(argv[2]);
            Molecule mol2 = Tools::LoadFile(argv[3]);

            Docking* docking = new Docking;
            docking->setHostStructure(mol1);
            docking->setGuestStructure(mol2);

            if (argc == 7) {
                double XXX = stod(std::string(argv[4]));
                double YYY = stod(std::string(argv[5]));
                double ZZZ = stod(std::string(argv[6]));

                std::cout << "Docking Position:" << XXX << " " << YYY << " " << ZZZ << std::endl;
                docking->setAnchorPosition(Position{ XXX, YYY, ZZZ });
            }

            docking->PerformDocking();

            docking->getMolecule().writeXYZFile("docked.xyz");

        } else if (strcmp(argv[1], "-hbonds") == 0) {
            if(argc != 6)
            {
                std::cerr << "Please use curcuma for hydrogen bond analysis as follows\ncurcuma -hbonds A.xyz index_donor index_proton index_acceptor" << std::endl;
                return -1;
            }

            std::cerr << "Opening file " << argv[2] << std::endl;
            std::ifstream input( argv[2] );
            std::vector<std::string> lines;
            int atoms = 0;
            int index = 0;
            int i = 0;
            bool xyzfile = std::string(argv[2]).find(".xyz") != std::string::npos;
            Molecule mol(atoms, 0);
            for( std::string line; getline( input, line ); )
            {
                if(index == 0 && xyzfile)
                {
                    atoms = stoi(line);
                    mol = Molecule(atoms, 0);
                }
                if(xyzfile)
                {
                    if(i > 1)
                    {
                        mol.setXYZ(line, i-2);
                    }
                    if(i-1 == atoms)
                    {
                        if(argc == 6)
                        {
                            if(std::string(argv[1]).find("-hbonds") != std::string::npos)
                            {
                               Distance(mol, argv);
                            }
                        }else
                        {
                            mol.print_geom();
                            std::cout << std::endl << std::endl;
                            std::cout << mol.getGeometry() << std::endl;
                        }
                        i = -1;
                        mol = Molecule(atoms, 0);
                    }
                    ++i;
                }else
                {
                    mol.setAtom(line, i);
                }
                index++;
            }

        } else if (strcmp(argv[1], "-confscan") == 0) {
            if (argc < 3) {
                std::cerr << "Please use curcuma for hydrogen bond analysis as follows\ncurcuma -confs conffile.xyz" << std::endl;
                return -1;
            }
            bool writeXYZ = false;
            int rank = 1e10;
            if (argc >= 4) {

                for (std::size_t i = 3; i < argc; ++i) {

                    if (strcmp(argv[i], "-rank") == 0) {
                        if (i + 1 < argc) {
                            rank = std::stoi(argv[i + 1]);
                            ++i;
                        }
                        ++i;
                    }

                    if (strcmp(argv[i], "-writeXYZ") == 0) {
                        writeXYZ = true;
                        ++i;
                    }
                }
            }

            std::cerr << "Opening file " << argv[2] << std::endl;

            ConfScan* scan = new ConfScan;
            scan->setFileName(argv[2]);
            scan->setMaxRank(rank);
            scan->setWriteXYZ(writeXYZ);
            scan->scan();

            return 0;
        } else {
            std::cerr << "Opening file " << argv[1] << std::endl;
            std::ifstream input(argv[1]);
            std::vector<std::string> lines;
            int atoms = 0;
            int index = 0;
            int i = 0;
            bool xyzfile = std::string(argv[1]).find(".xyz") != std::string::npos;
            Molecule mol(atoms, 0);
            for( std::string line; getline( input, line ); )
            {
                if(index == 0 && xyzfile)
                {
                    atoms = stoi(line);
                    mol = Molecule(atoms, 0);
                }
                if(xyzfile)
                {
                    if(i > 1)
                    {
                        mol.setXYZ(line, i-2);
                    }
                    if(i-1 == atoms)
                    {

                            mol.print_geom();
                            std::cout << std::endl << std::endl;
                            std::cout << mol.getGeometry() << std::endl
                                      << std::endl;
                            std::cout << "Centroid: " << mol.Centroid(true).transpose() << std::endl;
                            mol.CalculateRotationalConstants();
                            // std::cout << mol.Ia() << " " << mol.Ib() << " " << mol.Ic() << std::endl;

                            i = -1;
                            mol = Molecule(atoms, 0);
                    }
                    ++i;
                }else
                {
                    mol.setAtom(line, i);
                }
                index++;
            }
        }
    }
    return 0;
}
