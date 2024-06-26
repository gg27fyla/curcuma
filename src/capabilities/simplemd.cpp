/*
 * <Simple MD Module for Cucuma. >
 * Copyright (C) 2020 - 2024 Conrad Hübler <Conrad.Huebler@gmx.net>
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

#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "src/capabilities/curcumaopt.h"
#include "src/capabilities/rmsdtraj.h"

#include "src/core/elements.h"
#include "src/core/energycalculator.h"
#include "src/core/global.h"
#include "src/core/molecule.h"

#include "src/tools/geometry.h"

#include "external/CxxThreadPool/include/CxxThreadPool.h"

#ifdef USE_Plumed
#include "plumed2/src/wrapper/Plumed.h"
#endif
#include "simplemd.h"

SimpleMD::SimpleMD(const json& controller, bool silent)
    : CurcumaMethod(CurcumaMDJson, controller, silent)
{
    UpdateController(controller);
    m_interface = new EnergyCalculator(m_method, controller["md"]);
}

SimpleMD::~SimpleMD()
{
    for (int i = 0; i < m_unique_structures.size(); ++i)
        delete m_unique_structures[i];
}

void SimpleMD::LoadControlJson()
{
    m_method = Json2KeyWord<std::string>(m_defaults, "method");
    m_thermostat = Json2KeyWord<std::string>(m_defaults, "thermostat");
    m_plumed = Json2KeyWord<std::string>(m_defaults, "plumed");

    m_spin = Json2KeyWord<int>(m_defaults, "spin");
    m_charge = Json2KeyWord<int>(m_defaults, "charge");
    m_dT = Json2KeyWord<double>(m_defaults, "dT");
    m_maxtime = Json2KeyWord<double>(m_defaults, "MaxTime");
    m_T0 = Json2KeyWord<double>(m_defaults, "T");
    m_rmrottrans = Json2KeyWord<int>(m_defaults, "rmrottrans");
    m_nocenter = Json2KeyWord<bool>(m_defaults, "nocenter");
    m_dump = Json2KeyWord<int>(m_defaults, "dump");
    m_print = Json2KeyWord<int>(m_defaults, "print");
    m_max_top_diff = Json2KeyWord<int>(m_defaults, "MaxTopoDiff");
    m_seed = Json2KeyWord<int>(m_defaults, "seed");

    m_rmsd = Json2KeyWord<double>(m_defaults, "rmsd");
    m_hmass = Json2KeyWord<double>(m_defaults, "hmass");

    m_impuls = Json2KeyWord<double>(m_defaults, "impuls");
    m_impuls_scaling = Json2KeyWord<double>(m_defaults, "impuls_scaling");
    m_writeUnique = Json2KeyWord<bool>(m_defaults, "unique");
    m_opt = Json2KeyWord<bool>(m_defaults, "opt");
    m_scale_velo = Json2KeyWord<double>(m_defaults, "velo");
    m_rescue = Json2KeyWord<bool>(m_defaults, "rescue");
    m_coupling = Json2KeyWord<double>(m_defaults, "coupling");
    if (m_coupling < m_dT)
        m_coupling = m_dT;

    m_writerestart = Json2KeyWord<int>(m_defaults, "writerestart");
    m_respa = Json2KeyWord<int>(m_defaults, "respa");
    m_dipole = Json2KeyWord<bool>(m_defaults, "dipole");

    m_writeXYZ = Json2KeyWord<bool>(m_defaults, "writeXYZ");
    m_writeinit = Json2KeyWord<bool>(m_defaults, "writeinit");
    m_mtd = Json2KeyWord<bool>(m_defaults, "mtd");
    m_mtd_dT = Json2KeyWord<int>(m_defaults, "mtd_dT");
    if (m_mtd_dT < 0) {
        m_eval_mtd = true;
    } else {
        m_eval_mtd = false;
    }
    m_initfile = Json2KeyWord<std::string>(m_defaults, "initfile");
    m_norestart = Json2KeyWord<bool>(m_defaults, "norestart");
    m_dt2 = m_dT * m_dT;
    m_rm_COM = Json2KeyWord<double>(m_defaults, "rm_COM");
    int rattle = Json2KeyWord<int>(m_defaults, "rattle");
    m_rattle_maxiter = Json2KeyWord<int>(m_defaults, "rattle_maxiter");
    if (rattle == 1) {
        Integrator = [=](double* grad) {
            this->Rattle(grad);
        };
        m_rattle_tolerance = Json2KeyWord<double>(m_defaults, "rattle_tolerance");
        // m_coupling = m_dT;
        m_rattle = Json2KeyWord<int>(m_defaults, "rattle");
        std::cout << "Using rattle to constrain bonds!" << std::endl;
    } else {
        Integrator = [=](double* grad) {
            this->Verlet(grad);
        };
    }

    if (Json2KeyWord<bool>(m_defaults, "cleanenergy")) {
        Energy = [=](double* grad) -> double {
            return this->CleanEnergy(grad);
        };
        std::cout << "Energy Calculator will be set up for each step! Single steps are slower, but more reliable. Recommended for the combination of GFN2 and solvation." << std::endl;
    } else {
        Energy = [=](double* grad) -> double {
            return this->FastEnergy(grad);
        };
        std::cout << "Energy Calculator will NOT be set up for each step! Fast energy calculation! This is the default way and should not be changed unless the energy and gradient calculation are unstable (happens with GFN2 and solvation)." << std::endl;
    }

    if (Json2KeyWord<std::string>(m_defaults, "wall").compare("spheric") == 0) {
        if (Json2KeyWord<std::string>(m_defaults, "wall_type").compare("logfermi") == 0) {
            WallPotential = [=](double* grad) -> double {
                this->m_wall_potential = this->ApplySphericLogFermiWalls(grad);
                return m_wall_potential;
            };
        } else if (Json2KeyWord<std::string>(m_defaults, "wall_type").compare("harmonic") == 0) {
            WallPotential = [=](double* grad) -> double {
                this->m_wall_potential = this->ApplySphericHarmonicWalls(grad);
                return m_wall_potential;
            };
        } else {
            std::cout << "Did not understand wall potential input. Exit now!" << std::endl;
            exit(1);
        }
        std::cout << "Setting up spherical potential" << std::endl;

        InitialiseWalls();
    } else if (Json2KeyWord<std::string>(m_defaults, "wall").compare("rect") == 0) {
        if (Json2KeyWord<std::string>(m_defaults, "wall_type").compare("logfermi") == 0) {
            WallPotential = [=](double* grad) -> double {
                this->m_wall_potential = this->ApplyRectLogFermiWalls(grad);
                return m_wall_potential;
            };
        } else if (Json2KeyWord<std::string>(m_defaults, "wall_type").compare("harmonic") == 0) {
            WallPotential = [=](double* grad) -> double {
                this->m_wall_potential = this->ApplyRectHarmonicWalls(grad);
                return m_wall_potential;
            };

        } else {
            std::cout << "Did not understand wall potential input. Exit now!" << std::endl;
            exit(1);
        }
        std::cout << "Setting up rectangular potential" << std::endl;

        InitialiseWalls();
    } else
        WallPotential = [=](double* grad) -> double {
            return 0;
        };
    m_rm_COM_step = m_rm_COM / m_dT;
}

bool SimpleMD::Initialise()
{
    static std::random_device rd{};
    static std::mt19937 gen{ rd() };
    if (m_seed == -1) {
        const auto start = std::chrono::high_resolution_clock::now();
        m_seed = std::chrono::duration_cast<std::chrono::seconds>(start.time_since_epoch()).count();
    } else if (m_seed == 0)
        m_seed = m_T0 * m_mass.size();
    std::cout << "Random seed is " << m_seed << std::endl;
    gen.seed(m_seed);

    if (m_initfile.compare("none") != 0) {
        json md;
        std::ifstream restart_file(m_initfile);
        try {
            restart_file >> md;
        } catch (nlohmann::json::type_error& e) {
            throw 404;
        } catch (nlohmann::json::parse_error& e) {
            throw 404;
        }
        LoadRestartInformation(md);
        m_restart = true;
    } else if (!m_norestart)
        LoadRestartInformation();

    if (m_molecule.AtomCount() == 0)
        return false;

    if (!m_restart) {
        std::ofstream result_file;
        result_file.open(Basename() + ".trj.xyz");
        result_file.close();
    }
    m_natoms = m_molecule.AtomCount();
    m_molecule.setCharge(0);
    if (!m_nocenter) {
        std::cout << "Move stucture to the origin ... " << std::endl;
        m_molecule.setGeometry(GeometryTools::TranslateGeometry(m_molecule.getGeometry(), GeometryTools::Centroid(m_molecule.getGeometry()), Position{ 0, 0, 0 }));
    } else
        std::cout << "Move stucture NOT to the origin ... " << std::endl;

    m_mass = std::vector<double>(3 * m_natoms, 0);
    m_rmass = std::vector<double>(3 * m_natoms, 0);
    m_atomtype = std::vector<int>(m_natoms, 0);

    if (!m_restart) {
        m_current_geometry = std::vector<double>(3 * m_natoms, 0);
        m_velocities = std::vector<double>(3 * m_natoms, 0);
        m_currentStep = 0;
    }

    m_gradient = std::vector<double>(3 * m_natoms, 0);
    m_virial = std::vector<double>(3 * m_natoms, 0);

    if(m_opt)
    {
        json js = CurcumaOptJson;
        js = MergeJson(js, m_defaults);
        js["writeXYZ"] = false;
        js["method"] = m_method;
        /*
        try {
            js["threads"] = m_defaults["threads"].get<int>();
        }
        catch (const nlohmann::detail::type_error& error) {

           }*/
        CurcumaOpt optimise(js, true);
        optimise.addMolecule(&m_molecule);
        optimise.start();
        auto mol = optimise.Molecules();

        auto molecule = ((*mol)[0]);
        m_molecule.setGeometry(molecule.getGeometry());
        m_molecule.appendXYZFile(Basename() + ".opt.xyz");
    }

    for (int i = 0; i < m_natoms; ++i) {
        m_atomtype[i] = m_molecule.Atom(i).first;

        if (!m_restart) {
            Position pos = m_molecule.Atom(i).second;
            m_current_geometry[3 * i + 0] = pos(0) / 1;
            m_current_geometry[3 * i + 1] = pos(1) / 1;
            m_current_geometry[3 * i + 2] = pos(2) / 1;
        }
        if (m_atomtype[i] == 1) {
            m_mass[3 * i + 0] = Elements::AtomicMass[m_atomtype[i]] * m_hmass;
            m_mass[3 * i + 1] = Elements::AtomicMass[m_atomtype[i]] * m_hmass;
            m_mass[3 * i + 2] = Elements::AtomicMass[m_atomtype[i]] * m_hmass;

            m_rmass[3 * i + 0] = 1 / m_mass[3 * i + 0];
            m_rmass[3 * i + 1] = 1 / m_mass[3 * i + 1];
            m_rmass[3 * i + 2] = 1 / m_mass[3 * i + 2];
        } else {
            m_mass[3 * i + 0] = Elements::AtomicMass[m_atomtype[i]];
            m_mass[3 * i + 1] = Elements::AtomicMass[m_atomtype[i]];
            m_mass[3 * i + 2] = Elements::AtomicMass[m_atomtype[i]];

            m_rmass[3 * i + 0] = 1 / m_mass[3 * i + 0];
            m_rmass[3 * i + 1] = 1 / m_mass[3 * i + 1];
            m_rmass[3 * i + 2] = 1 / m_mass[3 * i + 2];
        }
    }
    if (!m_restart) {
        InitVelocities(m_scale_velo);
    }
    m_molecule.setCharge(m_charge);
    m_molecule.setSpin(m_spin);
    m_interface->setMolecule(m_molecule);

    if (m_writeUnique) {
        json rmsdtraj = RMSDTrajJson;
        rmsdtraj["writeUnique"] = true;
        rmsdtraj["rmsd"] = m_rmsd;
        rmsdtraj["writeRMSD"] = false;
        m_unqiue = new RMSDTraj(rmsdtraj, true);
        m_unqiue->setBaseName(Basename() + ".xyz");
        m_unqiue->Initialise();
    }
    m_dof = 3 * m_natoms;

    InitConstrainedBonds();
    if (m_writeinit) {
        json init = WriteRestartInformation();
        std::ofstream result_file;
        result_file.open(Basename() + ".init.json");
        result_file << init;
        result_file.close();
    }
    m_initialised = true;
    return true;
}

void SimpleMD::InitConstrainedBonds()
{
    if (m_rattle) {
        auto m = m_molecule.DistanceMatrix();
        m_topo_initial = m.second;
        for (int i = 0; i < m_molecule.AtomCount(); ++i) {
            for (int j = 0; j < i; ++j) {
                if (m.second(i, j)) {
                    if (m_rattle == 2) {
                        if (m_molecule.Atom(i).first != 1 && m_molecule.Atom(j).first != 1)
                            continue;
                    }
                    std::pair<int, int> indicies(i, j);
                    std::pair<double, double> minmax(m_molecule.CalculateDistance(i, j) * m_molecule.CalculateDistance(i, j), m_molecule.CalculateDistance(i, j) * m_molecule.CalculateDistance(i, j));
                    std::pair<std::pair<int, int>, double> bond(indicies, m_molecule.CalculateDistance(i, j) * m_molecule.CalculateDistance(i, j));
                    m_bond_constrained.push_back(std::pair<std::pair<int, int>, double>(bond));

                    // std::cout << i << " " << j << " " << bond.second << std::endl;
                }
            }
        }
    }
    std::cout << m_dof << " initial degrees of freedom " << std::endl;
    std::cout << m_bond_constrained.size() << " constrains active" << std::endl;
    m_dof -= m_bond_constrained.size();
    std::cout << m_dof << " degrees of freedom remaining ..." << std::endl;
}

void SimpleMD::InitVelocities(double scaling)
{
    static std::random_device rd{};
    static std::mt19937 gen{ rd() };
    std::normal_distribution<> d{ 0, 1 };
    double Px = 0.0, Py = 0.0, Pz = 0.0;
    for (int i = 0; i < m_natoms; ++i) {
        double v0 = sqrt(kb_Eh * m_T0 * amu2au / (m_mass[i])) * scaling / fs2amu;
        m_velocities[3 * i + 0] = v0 * d(gen);
        m_velocities[3 * i + 1] = v0 * d(gen);
        m_velocities[3 * i + 2] = v0 * d(gen);
        Px += m_velocities[3 * i + 0] * m_mass[i];
        Py += m_velocities[3 * i + 1] * m_mass[i];
        Pz += m_velocities[3 * i + 2] * m_mass[i];
    }
    for (int i = 0; i < m_natoms; ++i) {
        m_velocities[3 * i + 0] -= Px / (m_mass[i] * m_natoms);
        m_velocities[3 * i + 1] -= Py / (m_mass[i] * m_natoms);
        m_velocities[3 * i + 2] -= Pz / (m_mass[i] * m_natoms);
    }
}

void SimpleMD::InitialiseWalls()
{
    /*
    { "wall_xl", 0},
    { "wall_yl", 0},
    { "wall_zl", 0},
    { "wall_x_min", 0},
    { "wall_x_max", 0},
    { "wall_y_min", 0},
    { "wall_y_max", 0},
    { "wall_z_min", 0},
    { "wall_z_max", 0},*/
    m_wall_spheric_radius = Json2KeyWord<double>(m_defaults, "wall_spheric_radius");
    m_wall_temp = Json2KeyWord<double>(m_defaults, "wall_temp");
    m_wall_beta = Json2KeyWord<double>(m_defaults, "wall_beta");

    m_wall_x_min = Json2KeyWord<double>(m_defaults, "wall_x_min");
    m_wall_x_max = Json2KeyWord<double>(m_defaults, "wall_x_max");
    m_wall_y_min = Json2KeyWord<double>(m_defaults, "wall_y_min");
    m_wall_y_max = Json2KeyWord<double>(m_defaults, "wall_y_max");
    m_wall_z_min = Json2KeyWord<double>(m_defaults, "wall_z_min");
    m_wall_z_max = Json2KeyWord<double>(m_defaults, "wall_z_max");
    std::vector<double> box = m_molecule.GetBox();
    double radius = 0;
    if (m_wall_x_min - m_wall_x_max < 1) {
        m_wall_x_min = -box[0] * 0.75;
        m_wall_x_max = -1 * m_wall_x_min;
        radius = std::max(radius, box[0]);
    }

    if (m_wall_y_min - m_wall_y_max < 1) {
        m_wall_y_min = -box[1] * 0.75;
        m_wall_y_max = -1 * m_wall_y_min;
        radius = std::max(radius, box[1]);
    }

    if (m_wall_z_min - m_wall_z_max < 1) {
        m_wall_z_min = -box[2] * 0.75;
        m_wall_z_max = -1 * m_wall_z_min;
        radius = std::max(radius, box[2]);
    }

    if (m_wall_spheric_radius < radius) {
        m_wall_spheric_radius = radius + 5;
    }
}

nlohmann::json SimpleMD::WriteRestartInformation()
{
    nlohmann::json restart;
    restart["method"] = m_method;
    restart["thermostat"] = m_thermostat;
    restart["dT"] = m_dT;
    restart["MaxTime"] = m_maxtime;
    restart["T"] = m_T0;
    restart["currentStep"] = m_currentStep;
    restart["velocities"] = Tools::DoubleVector2String(m_velocities);
    restart["geometry"] = Tools::DoubleVector2String(m_current_geometry);
    restart["gradient"] = Tools::DoubleVector2String(m_gradient);
    restart["rmrottrans"] = m_rmrottrans;
    restart["nocenter"] = m_nocenter;
    restart["average_T"] = m_aver_Temp;
    restart["average_Epot"] = m_aver_Epot;
    restart["average_Ekin"] = m_aver_Ekin;
    restart["average_Etot"] = m_aver_Etot;
    restart["average_Virial"] = m_average_virial_correction;
    restart["average_Wall"] = m_average_wall_potential;

    restart["coupling"] = m_coupling;
    restart["MaxTopoDiff"] = m_max_top_diff;
    restart["impuls"] = m_impuls;
    restart["impuls_scaling"] = m_impuls_scaling;
    restart["respa"] = m_respa;
    restart["rm_COM"] = m_rm_COM;
    restart["mtd"] = m_mtd;

    return restart;
};

bool SimpleMD::LoadRestartInformation()
{
    if (!Restart())
        return false;
    StringList files = RestartFiles();
    int error = 0;
    for (const auto& f : files) {
        std::ifstream file(f);
        json restart;
        try {
            file >> restart;
        } catch (json::type_error& e) {
            error++;
            continue;
        } catch (json::parse_error& e) {
            error++;
            continue;
        }

        json md;
        try {
            md = restart[MethodName()[0]];
        } catch (json::type_error& e) {
            error++;
            continue;
        }
        return LoadRestartInformation(md);
    }
    return true;
};

bool SimpleMD::LoadRestartInformation(const json& state)
{
    std::string geometry, velocities, constrains;

    try {
        m_method = state["method"];
    } catch (json::type_error& e) {
    }
    try {
        m_dT = state["dT"];
    } catch (json::type_error& e) {
    }
    try {
        m_maxtime = state["MaxTime"];
    } catch (json::type_error& e) {
    }
    try {
        m_rmrottrans = state["rmrottrans"];
    } catch (json::type_error& e) {
    }
    try {
        m_nocenter = state["nocenter"];
    } catch (json::type_error& e) {
    }
    try {
        m_T0 = state["T"];
    } catch (json::type_error& e) {
    }
    try {
        m_currentStep = state["currentStep"];
    } catch (json::type_error& e) {
    }

    try {
        m_aver_Epot = state["average_Epot"];
    } catch (json::type_error& e) {
    }

    try {
        m_aver_Ekin = state["average_Ekin"];
    } catch (json::type_error& e) {
    }

    try {
        m_aver_Etot = state["average_Etot"];
    } catch (json::type_error& e) {
    }
    try {
        m_aver_Temp = state["average_T"];
    } catch (json::type_error& e) {
    }

    try {
        m_average_virial_correction = state["average_Virial"];
    } catch (json::type_error& e) {
    }

    try {
        m_average_wall_potential = state["average_Wall"];
    } catch (json::type_error& e) {
    }

    try {
        m_coupling = state["coupling"];
    } catch (json::type_error& e) {
    }

    try {
        m_respa = state["respa"];
    } catch (json::type_error& e) {
    }

    try {
        m_thermostat = state["thermostat"];
    } catch (json::type_error& e) {
    }

    try {
        geometry = state["geometry"];
    } catch (json::type_error& e) {
    }

    try {
        velocities = state["velocities"];
    } catch (json::type_error& e) {
    }
    if (geometry.size()) {
        m_current_geometry = Tools::String2DoubleVec(geometry, "|");
    }
    if (velocities.size()) {
        m_velocities = Tools::String2DoubleVec(velocities, "|");
    }
    m_restart = geometry.size() && velocities.size();

    return true;
}

void SimpleMD::start()
{
    if (m_initialised == false)
        return;
    auto unix_timestamp = std::chrono::seconds(std::time(NULL));
    m_unix_started = std::chrono::milliseconds(unix_timestamp).count();
    double* gradient = new double[3 * m_natoms];
    std::vector<json> states;
    for (int i = 0; i < 3 * m_natoms; ++i) {
        gradient[i] = 0;
    }

    if (m_thermostat.compare("csvr") == 0) {
        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "\nUsing Canonical sampling through velocity rescaling (CSVR) Thermostat\nJ. Chem. Phys. 126, 014101 (2007) - DOI: 10.1063/1.2408420\n\n");
        ThermostatFunction = std::bind(&SimpleMD::CSVR, this);
    } else if (m_thermostat.compare("berendson") == 0) {
        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "\nUsing Berendson Thermostat\nJ. Chem. Phys. 81, 3684 (1984) - DOI: 10.1063/1.448118\n\n");
        ThermostatFunction = std::bind(&SimpleMD::Berendson, this);
    } else {
        ThermostatFunction = std::bind(&SimpleMD::None, this);
        std::cout << "No Thermostat applied\n"
                  << std::endl;
    }

    m_Epot = Energy(gradient);
    m_Ekin = EKin();
    m_Etot = m_Epot + m_Ekin;

    int m_step = 0;

    PrintStatus();

#ifdef USE_Plumed
    plumed plumedmain;

    if (m_mtd) {
        plumedmain = plumed_create();
        int real_precision = 8;
        double energyUnits = 2625.5;
        double lengthUnits = 10;
        double timeUnits = 1e-3;
        double massUnits = 1;
        double chargeUnit = 1;
        int restart = m_restart;
        plumed_cmd(plumedmain, "setRealPrecision", &real_precision); // Pass a pointer to an integer containing the size of a real number (4 or 8)
        plumed_cmd(plumedmain, "setMDEnergyUnits", &energyUnits); // Pass a pointer to the conversion factor between the energy unit used in your code and kJ mol-1
        plumed_cmd(plumedmain, "setMDLengthUnits", &lengthUnits); // Pass a pointer to the conversion factor between the length unit used in your code and nm
        plumed_cmd(plumedmain, "setMDTimeUnits", &timeUnits); // Pass a pointer to the conversion factor between the time unit used in your code and ps
        plumed_cmd(plumedmain, "setNatoms", &m_natoms); // Pass a pointer to the number of atoms in the system to plumed
        plumed_cmd(plumedmain, "setMDEngine", "curcuma");
        plumed_cmd(plumedmain, "setMDMassUnits", &massUnits); // Pass a pointer to the conversion factor between the mass unit used in your code and amu
        plumed_cmd(plumedmain, "setMDChargeUnits", &chargeUnit);
        plumed_cmd(plumedmain, "setTimestep", &m_dT); // Pass a pointer to the molecular dynamics timestep to plumed                       // Pass the name of your md engine to plumed (now it is just a label)
        plumed_cmd(plumedmain, "setKbT", &kb_Eh);
        plumed_cmd(plumedmain, "setLogFile", "plumed_log.out"); // Pass the file  on which to write out the plumed log (to be created)
        plumed_cmd(plumedmain, "setRestart", &restart); // Pointer to an integer saying if we are restarting (zero means no, one means yes)
        plumed_cmd(plumedmain, "init", NULL);
        plumed_cmd(plumedmain, "read", m_plumed.c_str());
        plumed_cmd(plumedmain, "setStep", &m_step);
        plumed_cmd(plumedmain, "setPositions", &m_current_geometry[0]);
        plumed_cmd(plumedmain, "setEnergy", &m_Epot);
        plumed_cmd(plumedmain, "setForces", &m_gradient[0]);
        plumed_cmd(plumedmain, "setVirial", &m_virial[0]);
        plumed_cmd(plumedmain, "setMasses", &m_mass[0]);
        plumed_cmd(plumedmain, "prepareCalc", NULL);
        plumed_cmd(plumedmain, "performCalc", NULL);
    }
#endif
    std::vector<double> charge(0, m_natoms);

#ifdef GCC
    //         std::cout << fmt::format("{0: ^{0}} {1: ^{1}} {2: ^{2}} {3: ^{3}} {4: ^{4}}\n", "Step", "Epot", "Ekin", "Etot", "T");
    // std::cout << fmt::format("{1: ^{0}} {1: ^{1}} {1: ^{2}} {1: ^{3}} {1: ^{4}}\n", "", "Eh", "Eh", "Eh", "K");
#else
    std::cout << "Step"
              << "\t"
              << "Epot"
              << "\t"
              << "Ekin"
              << "\t"
              << "Etot"
              << "\t"
              << "T" << std::endl;
    std::cout << "  "
              << "\t"
              << "Eh"
              << "\t"
              << "Eh"
              << "\t"
              << "Eh"
              << "\t"
              << "T" << std::endl;
#endif

    for (; m_currentStep < m_maxtime;) {
        auto step0 = std::chrono::system_clock::now();

        if (CheckStop() == true) {
            TriggerWriteRestart();
#ifdef USE_Plumed
            if (m_mtd) {
                plumed_finalize(plumedmain); // Call the plumed destructor
            }
#endif
            return;
        }

        if (m_rm_COM_step > 0 && m_step % m_rm_COM_step == 0) {
            // std::cout << "Removing COM motion." << std::endl;
            if (m_rmrottrans == 1)
                RemoveRotation(m_velocities);
            else if (m_rmrottrans == 2)
                RemoveRotations(m_velocities);
            else if (m_rmrottrans == 3) {
                RemoveRotations(m_velocities);
                RemoveRotation(m_velocities);
            }
        }
        WallPotential(gradient);
        Integrator(gradient);

        ThermostatFunction();
        m_Ekin = EKin();

#ifdef USE_Plumed
        if (m_mtd) {
            plumed_cmd(plumedmain, "setStep", &m_step);

            plumed_cmd(plumedmain, "setPositions", &m_current_geometry[0]);

            plumed_cmd(plumedmain, "setEnergy", &m_Epot);
            plumed_cmd(plumedmain, "setForces", &m_gradient[0]);
            plumed_cmd(plumedmain, "setVirial", &m_virial[0]);

            plumed_cmd(plumedmain, "setMasses", &m_mass[0]);
            if (m_eval_mtd) {
                plumed_cmd(plumedmain, "prepareCalc", NULL);
                plumed_cmd(plumedmain, "performCalc", NULL);
            } else {
                if (std::abs(m_T0 - m_aver_Temp) < m_mtd_dT && m_step > 10) {
                    m_eval_mtd = true;
                    std::cout << "Starting with MetaDynamics ..." << std::endl;
                }
            }
        }
#endif

        if (m_step % m_dump == 0) {
            bool write = WriteGeometry();
            if (write) {
                states.push_back(WriteRestartInformation());
                m_current_rescue = 0;
            } else if (!write && m_rescue && states.size() > (1 - m_current_rescue)) {
                std::cout << "Molecule exploded, resetting to previous state ..." << std::endl;
                LoadRestartInformation(states[states.size() - 1 - m_current_rescue]);
                Geometry geometry = m_molecule.getGeometry();
                for (int i = 0; i < m_natoms; ++i) {
                    geometry(i, 0) = m_current_geometry[3 * i + 0] * au;
                    geometry(i, 1) = m_current_geometry[3 * i + 1] * au;
                    geometry(i, 2) = m_current_geometry[3 * i + 2] * au;
                }
                m_molecule.setGeometry(geometry);
                m_molecule.GetFragments();
                InitVelocities(-1);
                Energy(gradient);
                m_Ekin = EKin();
                m_Etot = m_Epot + m_Ekin;
                m_current_rescue++;
                PrintStatus();
                m_time_step = 0;
            }
        }

        if (m_unstable || m_interface->Error() || m_interface->HasNan()) {
            PrintStatus();
            fmt::print(fg(fmt::color::salmon) | fmt::emphasis::bold, "Simulation got unstable, exiting!\n");

            std::ofstream restart_file("unstable_curcuma.json");
            restart_file << WriteRestartInformation() << std::endl;
            m_time_step = 0;
#ifdef USE_Plumed
            if (m_mtd) {
                plumed_finalize(plumedmain); // Call the plumed destructor
            }
#endif
            return;
        }

        if (m_writerestart > -1 && m_step % m_writerestart == 0) {
            std::ofstream restart_file("curcuma_step_" + std::to_string(int(m_step * m_dT)) + ".json");
            nlohmann::json restart;
            restart_file << WriteRestartInformation() << std::endl;
        }
        if ((m_step && int(m_step * m_dT) % m_print == 0)) {
            m_Etot = m_Epot + m_Ekin;
            PrintStatus();
            m_time_step = 0;
        }

        if (m_impuls > m_T) {
            InitVelocities(m_scale_velo * m_impuls_scaling);
            m_Ekin = EKin();
            // PrintStatus();
            m_time_step = 0;
        }

        if (m_current_rescue >= m_max_rescue) {
            fmt::print(fg(fmt::color::salmon) | fmt::emphasis::bold, "Nothing really helps");
            break;
        }
        m_step++;
        m_currentStep += m_dT;
        m_time_step += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - step0).count();
    }
    PrintStatus();
    if (m_thermostat.compare("csvr") == 0)
        std::cout << "Exchange with heat bath " << m_Ekin_exchange << "Eh" << std::endl;
    if (m_dipole) {
        /*
        double dipole = 0.0;
        for( auto d : m_collected_dipole)
            dipole += d;
        dipole /= m_collected_dipole.size();
        std::cout << dipole*2.5418 << " average dipole in Debye and " << dipole*2.5418*3.3356e-30 << " Cm" << std::endl;
        */
        std::cout << "Calculated averaged dipole moment " << m_aver_dipol * 2.5418 << " Debye and " << m_aver_dipol * 2.5418 * 3.3356 << " Cm [e-30]" << std::endl;
    }

#ifdef USE_Plumed
    if (m_mtd) {
        plumed_finalize(plumedmain); // Call the plumed destructor
    }
#endif
    std::ofstream restart_file("curcuma_final.json");
    restart_file << WriteRestartInformation() << std::endl;
    std::remove("curcuma_restart.json");
    delete[] gradient;
}

void SimpleMD::Verlet(double* grad)
{
    for (int i = 0; i < m_natoms; ++i) {
        m_current_geometry[3 * i + 0] = m_current_geometry[3 * i + 0] + m_dT * m_velocities[3 * i + 0] - 0.5 * grad[3 * i + 0] * m_rmass[3 * i + 0] * m_dt2;
        m_current_geometry[3 * i + 1] = m_current_geometry[3 * i + 1] + m_dT * m_velocities[3 * i + 1] - 0.5 * grad[3 * i + 1] * m_rmass[3 * i + 1] * m_dt2;
        m_current_geometry[3 * i + 2] = m_current_geometry[3 * i + 2] + m_dT * m_velocities[3 * i + 2] - 0.5 * grad[3 * i + 2] * m_rmass[3 * i + 2] * m_dt2;

        m_velocities[3 * i + 0] = m_velocities[3 * i + 0] - 0.5 * m_dT * grad[3 * i + 0] * m_rmass[3 * i + 0];
        m_velocities[3 * i + 1] = m_velocities[3 * i + 1] - 0.5 * m_dT * grad[3 * i + 1] * m_rmass[3 * i + 1];
        m_velocities[3 * i + 2] = m_velocities[3 * i + 2] - 0.5 * m_dT * grad[3 * i + 2] * m_rmass[3 * i + 2];
    }
    m_Epot = Energy(grad);
    double ekin = 0.0;

    for (int i = 0; i < m_natoms; ++i) {
        m_velocities[3 * i + 0] -= 0.5 * m_dT * grad[3 * i + 0] * m_rmass[3 * i + 0];
        m_velocities[3 * i + 1] -= 0.5 * m_dT * grad[3 * i + 1] * m_rmass[3 * i + 1];
        m_velocities[3 * i + 2] -= 0.5 * m_dT * grad[3 * i + 2] * m_rmass[3 * i + 2];

        ekin += m_mass[i] * (m_velocities[3 * i] * m_velocities[3 * i] + m_velocities[3 * i + 1] * m_velocities[3 * i + 1] + m_velocities[3 * i + 2] * m_velocities[3 * i + 2]);
        m_gradient[3 * i + 0] = grad[3 * i + 0];
        m_gradient[3 * i + 1] = grad[3 * i + 1];
        m_gradient[3 * i + 2] = grad[3 * i + 2];
    }
    ekin *= 0.5;
    double T = 2.0 * ekin / (kb_Eh * m_dof);
    m_unstable = T > 100 * m_T;
    m_T = T;
}

void SimpleMD::Rattle(double* grad)
{
    /* this part was adopted from
     * Numerische Simulation in der Moleküldynamik
     * by
     * Griebel, Knapek, Zumbusch, Caglar
     * 2003, Springer-Verlag
     * and from
     * Molecular Simulation of Fluids
     * by Richard J. Sadus
     * some suff was just ignored or corrected
     * like dT^3 -> dT^2 and
     * updated velocities of the second atom (minus instead of plus)
     */
    double* coord = new double[3 * m_natoms];
    double m_dT_inverse = 1 / m_dT;
    std::vector<int> moved(m_natoms, 0);
    for (int i = 0; i < m_natoms; ++i) {
        coord[3 * i + 0] = m_current_geometry[3 * i + 0] + m_dT * m_velocities[3 * i + 0] - 0.5 * grad[3 * i + 0] * m_rmass[3 * i + 0] * m_dt2;
        coord[3 * i + 1] = m_current_geometry[3 * i + 1] + m_dT * m_velocities[3 * i + 1] - 0.5 * grad[3 * i + 1] * m_rmass[3 * i + 1] * m_dt2;
        coord[3 * i + 2] = m_current_geometry[3 * i + 2] + m_dT * m_velocities[3 * i + 2] - 0.5 * grad[3 * i + 2] * m_rmass[3 * i + 2] * m_dt2;

        m_velocities[3 * i + 0] -= 0.5 * m_dT * grad[3 * i + 0] * m_rmass[3 * i + 0];
        m_velocities[3 * i + 1] -= 0.5 * m_dT * grad[3 * i + 1] * m_rmass[3 * i + 1];
        m_velocities[3 * i + 2] -= 0.5 * m_dT * grad[3 * i + 2] * m_rmass[3 * i + 2];
    }
    double iter = 0;
    double max_mu = 10;

    while (iter < m_rattle_maxiter) {
        iter++;
        for (auto bond : m_bond_constrained) {
            int i = bond.first.first, j = bond.first.second;
            double distance = bond.second;
            double distance_current = ((coord[3 * i + 0] - coord[3 * j + 0]) * (coord[3 * i + 0] - coord[3 * j + 0])
                + (coord[3 * i + 1] - coord[3 * j + 1]) * (coord[3 * i + 1] - coord[3 * j + 1])
                + (coord[3 * i + 2] - coord[3 * j + 2]) * (coord[3 * i + 2] - coord[3 * j + 2]));

            if (std::abs(distance - distance_current) > 2 * m_rattle_tolerance * distance) {
                double r = distance - distance_current;

                double dx = m_current_geometry[3 * i + 0] - m_current_geometry[3 * j + 0];
                double dy = m_current_geometry[3 * i + 1] - m_current_geometry[3 * j + 1];
                double dz = m_current_geometry[3 * i + 2] - m_current_geometry[3 * j + 2];

                double scalarproduct = (dx) * (coord[3 * i + 0] - coord[3 * j + 0])
                    + (dy) * (coord[3 * i + 1] - coord[3 * j + 1])
                    + (dz) * (coord[3 * i + 2] - coord[3 * j + 2]);
                if (scalarproduct >= m_rattle_tolerance * distance) {
                    moved[i] = 1;
                    moved[j] = 1;

                    double lambda = r / (1 * (m_rmass[i] + m_rmass[j]) * scalarproduct);
                    while (std::abs(lambda) > max_mu)
                        lambda /= 2;
                    coord[3 * i + 0] += dx * lambda * 0.5 * m_rmass[i];
                    coord[3 * i + 1] += dy * lambda * 0.5 * m_rmass[i];
                    coord[3 * i + 2] += dz * lambda * 0.5 * m_rmass[i];

                    coord[3 * j + 0] -= dx * lambda * 0.5 * m_rmass[j];
                    coord[3 * j + 1] -= dy * lambda * 0.5 * m_rmass[j];
                    coord[3 * j + 2] -= dz * lambda * 0.5 * m_rmass[j];

                    m_velocities[3 * i + 0] += dx * lambda * 0.5 * m_rmass[i] * m_dT_inverse;
                    m_velocities[3 * i + 1] += dy * lambda * 0.5 * m_rmass[i] * m_dT_inverse;
                    m_velocities[3 * i + 2] += dz * lambda * 0.5 * m_rmass[i] * m_dT_inverse;

                    m_velocities[3 * j + 0] -= dx * lambda * 0.5 * m_rmass[j] * m_dT_inverse;
                    m_velocities[3 * j + 1] -= dy * lambda * 0.5 * m_rmass[j] * m_dT_inverse;
                    m_velocities[3 * j + 2] -= dz * lambda * 0.5 * m_rmass[j] * m_dT_inverse;
                }
            }
        }
    }
    for (int i = 0; i < m_natoms; ++i) {
        m_current_geometry[3 * i + 0] = coord[3 * i + 0];
        m_current_geometry[3 * i + 1] = coord[3 * i + 1];
        m_current_geometry[3 * i + 2] = coord[3 * i + 2];
    }
    m_Epot = Energy(grad);
    double ekin = 0.0;

    for (int i = 0; i < m_natoms; ++i) {
        m_velocities[3 * i + 0] -= 0.5 * m_dT * grad[3 * i + 0] * m_rmass[3 * i + 0];
        m_velocities[3 * i + 1] -= 0.5 * m_dT * grad[3 * i + 1] * m_rmass[3 * i + 1];
        m_velocities[3 * i + 2] -= 0.5 * m_dT * grad[3 * i + 2] * m_rmass[3 * i + 2];

        m_gradient[3 * i + 0] = grad[3 * i + 0];
        m_gradient[3 * i + 1] = grad[3 * i + 1];
        m_gradient[3 * i + 2] = grad[3 * i + 2];
    }
    m_virial_correction = 0;
    iter = 0;
    ekin = 0.0;
    while (iter < m_rattle_maxiter) {
        iter++;
        for (auto bond : m_bond_constrained) {
            int i = bond.first.first, j = bond.first.second;
            if (moved[i] == 1 && moved[j] == 1) {
                double distance = bond.second;

                double dx = coord[3 * i + 0] - coord[3 * j + 0];
                double dy = coord[3 * i + 1] - coord[3 * j + 1];
                double dz = coord[3 * i + 2] - coord[3 * j + 2];
                double dvx = m_velocities[3 * i + 0] - m_velocities[3 * j + 0];
                double dvy = m_velocities[3 * i + 1] - m_velocities[3 * j + 1];
                double dvz = m_velocities[3 * i + 2] - m_velocities[3 * j + 2];

                double r = (dx) * (dvx) + (dy) * (dvy) + (dz) * (dvz);

                double mu = -1 * r / ((m_rmass[i] + m_rmass[j]) * distance);
                while (std::abs(mu) > max_mu)
                    mu /= 2;
                if (std::abs(mu) > m_rattle_tolerance && std::abs(mu) < max_mu) {
                    m_virial_correction += mu * distance;
                    m_velocities[3 * i + 0] += dx * mu * m_rmass[i];
                    m_velocities[3 * i + 1] += dy * mu * m_rmass[i];
                    m_velocities[3 * i + 2] += dz * mu * m_rmass[i];

                    m_velocities[3 * j + 0] -= dx * mu * m_rmass[j];
                    m_velocities[3 * j + 1] -= dy * mu * m_rmass[j];
                    m_velocities[3 * j + 2] -= dz * mu * m_rmass[j];
                }
            }
        }
    }
    delete[] coord;
    for (int i = 0; i < m_natoms; ++i) {
        ekin += m_mass[i] * (m_velocities[3 * i] * m_velocities[3 * i] + m_velocities[3 * i + 1] * m_velocities[3 * i + 1] + m_velocities[3 * i + 2] * m_velocities[3 * i + 2]);
    }
    ekin *= 0.5;
    double T = 2.0 * ekin / (kb_Eh * m_dof);
    m_unstable = T > 10000 * m_T;
    m_T = T;
}

void SimpleMD::Rattle_Verlet_First(double* coord, double* grad)
{
}

void SimpleMD::Rattle_Constrain_First(double* coord, double* grad)
{
}

void SimpleMD::Rattle_Verlet_Second(double* coord, double* grad)
{
}

double SimpleMD::ApplySphericLogFermiWalls(double* grad)
{
    double potential = 0;
    double kbT = m_wall_temp * kb_Eh;
    // int counter = 0;
    for (int i = 0; i < m_natoms; ++i) {
        double distance = sqrt(m_current_geometry[3 * i + 0] * m_current_geometry[3 * i + 0] + m_current_geometry[3 * i + 1] * m_current_geometry[3 * i + 1] + m_current_geometry[3 * i + 2] * m_current_geometry[3 * i + 2]);
        double exp_expr = exp(m_wall_beta * (distance - m_wall_spheric_radius));
        double curr_pot = kbT * log(1 + exp_expr);
        // counter += distance > m_wall_radius;
        // std::cout << m_wall_beta*m_current_geometry[3 * i + 0]*exp_expr/(distance*(1-exp_expr)) << " ";
        grad[3 * i + 0] -= kbT * m_wall_beta * m_current_geometry[3 * i + 0] * exp_expr / (distance * (1 - exp_expr));
        grad[3 * i + 1] -= kbT * m_wall_beta * m_current_geometry[3 * i + 1] * exp_expr / (distance * (1 - exp_expr));
        grad[3 * i + 2] -= kbT * m_wall_beta * m_current_geometry[3 * i + 2] * exp_expr / (distance * (1 - exp_expr));

        // std::cout << distance << " ";
        potential += curr_pot;
    }
    //    std::cout << counter << " ";
    return potential;
    // std::cout << potential*kbT << std::endl;
}

double SimpleMD::ApplyRectLogFermiWalls(double* grad)
{
    double potential = 0;
    double kbT = m_wall_temp * kb_Eh;
    int counter = 0;
    double b = m_wall_beta;
    double sum_grad = 0;
    for (int i = 0; i < m_natoms; ++i) {
        double exp_expr_xl = exp(b * (m_wall_x_min - m_current_geometry[3 * i + 0]));
        double exp_expr_xu = exp(b * (m_current_geometry[3 * i + 0] - m_wall_x_max));

        double exp_expr_yl = exp(b * (m_wall_y_min - m_current_geometry[3 * i + 1]));
        double exp_expr_yu = exp(b * (m_current_geometry[3 * i + 1] - m_wall_y_max));

        double exp_expr_zl = exp(b * (m_wall_z_min - m_current_geometry[3 * i + 2]));
        double exp_expr_zu = exp(b * (m_current_geometry[3 * i + 2] - m_wall_z_max));

        double curr_pot = kbT * (log(1 + exp_expr_xl) + log(1 + exp_expr_xu) + log(1 + exp_expr_yl) + log(1 + exp_expr_yu) + log(1 + exp_expr_zl) + log(1 + exp_expr_zu));
        counter += (m_current_geometry[3 * i + 0] - m_wall_x_min) < 0 || (m_wall_x_max - m_current_geometry[3 * i + 0]) < 0 || (m_current_geometry[3 * i + 1] - m_wall_y_min) < 0 || (m_wall_y_max - m_current_geometry[3 * i + 1]) < 0 || (m_current_geometry[3 * i + 2] - m_wall_z_min) < 0 || (m_wall_z_max - m_current_geometry[3 * i + 2]) < 0;
        // std::cout << i << " " << counter << std::endl;

        // std::cout << m_wall_beta*m_current_geometry[3 * i + 0]*exp_expr/(distance*(1-exp_expr)) << " ";
        if (i == 81) {
            //    std::cout << std::endl;
            //    std::cout << m_current_geometry[3 * i + 0] << " " << m_current_geometry[3 * i + 1] << " " << m_current_geometry[3 * i + 2] << std::endl;
            //    std::cout << grad[3 * i + 0] << " " << grad[3 * i + 1] << " " <<grad[3 * i + 2] << std::endl;
        }
        grad[3 * i + 0] += kbT * b * (exp_expr_xu / (1 - exp_expr_xu) - exp_expr_xl / (1 - exp_expr_xl)); // m_current_geometry[3 * i + 0]*exp_expr/(distance*(1-exp_expr));
        grad[3 * i + 1] += kbT * b * (exp_expr_yu / (1 - exp_expr_yu) - exp_expr_yl / (1 - exp_expr_yl));
        grad[3 * i + 2] += kbT * b * (exp_expr_zu / (1 - exp_expr_zu) - exp_expr_zl / (1 - exp_expr_zl));
        sum_grad += grad[3 * i + 0] + grad[3 * i + 1] + grad[3 * i + 2];
        // if( i == 81)
        {
            // std::cout << i << " " <<grad[3 * i + 0] << " " << grad[3 * i + 1] << " " <<grad[3 * i + 2] << std::endl;
        }
        // std::cout << distance << " ";
        potential += curr_pot;
    }
    std::cout << counter << " " << sum_grad;
    return potential;
    // std::cout << potential*kbT << std::endl;
}

double SimpleMD::ApplySphericHarmonicWalls(double* grad)
{
    double potential = 0;
    double k = m_wall_temp * kb_Eh;
    int counter = 0;
    for (int i = 0; i < m_natoms; ++i) {
        double distance = sqrt(m_current_geometry[3 * i + 0] * m_current_geometry[3 * i + 0] + m_current_geometry[3 * i + 1] * m_current_geometry[3 * i + 1] + m_current_geometry[3 * i + 2] * m_current_geometry[3 * i + 2]);
        double curr_pot = 0.5 * k * (m_wall_spheric_radius - distance) * (m_wall_spheric_radius - distance) * (distance > m_wall_spheric_radius);
        double out = distance > m_wall_spheric_radius;
        counter += out;

        double diff = k * (m_wall_spheric_radius - distance) * (distance > m_wall_spheric_radius);

        double dx = diff * m_current_geometry[3 * i + 0] / distance;
        double dy = diff * m_current_geometry[3 * i + 1] / distance;
        double dz = diff * m_current_geometry[3 * i + 2] / distance;

        grad[3 * i + 0] -= dx;
        grad[3 * i + 1] -= dy;
        grad[3 * i + 2] -= dz;
        /*
        if(out)
        {
            std::cout << m_current_geometry[3 * i + 0]  << " " << m_current_geometry[3 * i + 1]  << " " << m_current_geometry[3 * i + 2] << std::endl;
            std::cout << dx << " " << dy << " " << dz << std::endl;
        }*/
        // std::cout << distance << " ";
        potential += curr_pot;
    }
    // std::cout << counter << " ";
    return potential;
    // std::cout << potential*kbT << std::endl;
}

double SimpleMD::ApplyRectHarmonicWalls(double* grad)
{
    double potential = 0;
    double k = m_wall_temp * kb_Eh;
    int counter = 0;
    double b = m_wall_beta;
    double sum_grad = 0;
    for (int i = 0; i < m_natoms; ++i) {
        double Vx = (m_current_geometry[3 * i + 0] - m_wall_x_min) * (m_current_geometry[3 * i + 0] - m_wall_x_min) * (m_current_geometry[3 * i + 0] < m_wall_x_min)
            + (m_current_geometry[3 * i + 0] - m_wall_x_max) * (m_current_geometry[3 * i + 0] - m_wall_x_max) * (m_current_geometry[3 * i + 0] > m_wall_x_max);

        double Vy = (m_current_geometry[3 * i + 1] - m_wall_y_min) * (m_current_geometry[3 * i + 1] - m_wall_y_min) * (m_current_geometry[3 * i + 1] < m_wall_y_min)
            + (m_current_geometry[3 * i + 1] - m_wall_y_max) * (m_current_geometry[3 * i + 1] - m_wall_y_max) * (m_current_geometry[3 * i + 1] > m_wall_y_max);

        double Vz = (m_current_geometry[3 * i + 2] - m_wall_z_min) * (m_current_geometry[3 * i + 2] - m_wall_z_min) * (m_current_geometry[3 * i + 2] < m_wall_z_min)
            + (m_current_geometry[3 * i + 2] - m_wall_z_max) * (m_current_geometry[3 * i + 2] - m_wall_z_max) * (m_current_geometry[3 * i + 2] > m_wall_z_max);

        double curr_pot = 0.5 * k * (Vx + Vy + Vz);
        int out = (m_current_geometry[3 * i + 0] - m_wall_x_min) < 0 || (m_wall_x_max - m_current_geometry[3 * i + 0]) < 0 || (m_current_geometry[3 * i + 1] - m_wall_y_min) < 0 || (m_wall_y_max - m_current_geometry[3 * i + 1]) < 0 || (m_current_geometry[3 * i + 2] - m_wall_z_min) < 0 || (m_wall_z_max - m_current_geometry[3 * i + 2]) < 0;
        counter += out;

        // std::cout << i << " " << counter << std::endl;

        double dx = k * (std::abs(m_current_geometry[3 * i + 0] - m_wall_x_min) * (m_current_geometry[3 * i + 0] < m_wall_x_min) - (m_current_geometry[3 * i + 0] - m_wall_x_max) * (m_current_geometry[3 * i + 0] > m_wall_x_max));

        double dy = k * (std::abs(m_current_geometry[3 * i + 1] - m_wall_y_min) * (m_current_geometry[3 * i + 1] < m_wall_y_min) - (m_current_geometry[3 * i + 1] - m_wall_y_max) * (m_current_geometry[3 * i + 1] > m_wall_y_max));

        double dz = k * (std::abs(m_current_geometry[3 * i + 2] - m_wall_z_min) * (m_current_geometry[3 * i + 2] < m_wall_z_min) - (m_current_geometry[3 * i + 2] - m_wall_z_max) * (m_current_geometry[3 * i + 2] > m_wall_z_max));
        grad[3 * i + 0] -= dx;
        grad[3 * i + 1] -= dy;
        grad[3 * i + 2] -= dz;
        /* if(out)
         {
             std::cout << m_current_geometry[3 * i + 0]  << " " << m_current_geometry[3 * i + 1]  << " " << m_current_geometry[3 * i + 2] << std::endl;
             std::cout << dx << " " << dy << " " << dz << std::endl;
         }*/
        sum_grad += dx + dy + dz;

        potential += curr_pot;
    }
    // std::cout << counter << " " << sum_grad;
    return potential;
    // std::cout << potential*kbT << std::endl;
}

void SimpleMD::RemoveRotations(std::vector<double>& velo)
{
    /*
     * This code was taken and adopted from the xtb sources
     * https://github.com/grimme-lab/xtb/blob/main/src/rmrottr.f90
     * Special thanks to the developers
     */
    double mass = 0;
    Position pos = { 0, 0, 0 }, angom{ 0, 0, 0 };
    Geometry geom(m_natoms, 3);

    std::vector<std::vector<int>> fragments = m_molecule.GetFragments();
    // std::cout << fragments.size() << std::endl;
    for (int f = 0; f < fragments.size(); ++f) {
        for (int i : fragments[f]) {
            double m = m_mass[i];
            mass += m;
            pos(0) += m * m_current_geometry[3 * i + 0];
            pos(1) += m * m_current_geometry[3 * i + 1];
            pos(2) += m * m_current_geometry[3 * i + 2];

            geom(i, 0) = m_current_geometry[3 * i + 0];
            geom(i, 1) = m_current_geometry[3 * i + 1];
            geom(i, 2) = m_current_geometry[3 * i + 2];
        }
        pos(0) /= mass;
        pos(1) /= mass;
        pos(2) /= mass;

        Geometry matrix = Geometry::Zero(3, 3);
        for (int i : fragments[f]) {
            double m = m_mass[i];
            geom(i, 0) -= pos(0);
            geom(i, 1) -= pos(1);
            geom(i, 2) -= pos(2);

            double x = geom(i, 0);
            double y = geom(i, 1);
            double z = geom(i, 2);
            angom(0) += m_mass[i] * (geom(i, 1) * velo[3 * i + 2] - geom(i, 2) * velo[3 * i + 1]);
            angom(1) += m_mass[i] * (geom(i, 2) * velo[3 * i + 0] - geom(i, 0) * velo[3 * i + 2]);
            angom(2) += m_mass[i] * (geom(i, 0) * velo[3 * i + 1] - geom(i, 1) * velo[3 * i + 0]);
            double x2 = x * x;
            double y2 = y * y;
            double z2 = z * z;
            matrix(0, 0) += m * (y2 + z2);
            matrix(1, 1) += m * (x2 + z2);
            matrix(2, 2) += m * (x2 + y2);
            matrix(0, 1) -= m * x * y;
            matrix(0, 2) -= m * x * z;
            matrix(1, 2) -= m * y * z;
        }
        matrix(1, 0) = matrix(0, 1);
        matrix(2, 0) = matrix(0, 2);
        matrix(2, 1) = matrix(1, 2);

        Position omega = matrix.inverse() * angom;

        Position rlm = { 0, 0, 0 }, ram = { 0, 0, 0 };
        for (int i : fragments[f]) {
            rlm(0) = rlm(0) + m_mass[i] * velo[3 * i + 0];
            rlm(1) = rlm(1) + m_mass[i] * velo[3 * i + 1];
            rlm(2) = rlm(2) + m_mass[i] * velo[3 * i + 2];
        }

        for (int i : fragments[f]) {
            ram(0) = (omega(1) * geom(i, 2) - omega(2) * geom(i, 1));
            ram(1) = (omega(2) * geom(i, 0) - omega(0) * geom(i, 2));
            ram(2) = (omega(0) * geom(i, 1) - omega(1) * geom(i, 0));

            velo[3 * i + 0] = velo[3 * i + 0] - rlm(0) / mass - ram(0);
            velo[3 * i + 1] = velo[3 * i + 1] - rlm(1) / mass - ram(1);
            velo[3 * i + 2] = velo[3 * i + 2] - rlm(2) / mass - ram(2);
        }
    }
}

void SimpleMD::RemoveRotation(std::vector<double>& velo)
{
    /*
     * This code was taken and adopted from the xtb sources
     * https://github.com/grimme-lab/xtb/blob/main/src/rmrottr.f90
     * Special thanks to the developers
     */
    double mass = 0;
    Position pos = { 0, 0, 0 }, angom{ 0, 0, 0 };
    Geometry geom(m_natoms, 3);

    for (int i = 0; i < m_natoms; ++i) {
        double m = m_mass[i];
        mass += m;
        pos(0) += m * m_current_geometry[3 * i + 0];
        pos(1) += m * m_current_geometry[3 * i + 1];
        pos(2) += m * m_current_geometry[3 * i + 2];

        geom(i, 0) = m_current_geometry[3 * i + 0];
        geom(i, 1) = m_current_geometry[3 * i + 1];
        geom(i, 2) = m_current_geometry[3 * i + 2];
    }
    pos(0) /= mass;
    pos(1) /= mass;
    pos(2) /= mass;

    Geometry matrix = Geometry::Zero(3, 3);
    for (int i = 0; i < m_natoms; ++i) {
        double m = m_mass[i];
        geom(i, 0) -= pos(0);
        geom(i, 1) -= pos(1);
        geom(i, 2) -= pos(2);

        double x = geom(i, 0);
        double y = geom(i, 1);
        double z = geom(i, 2);
        angom(0) += m_mass[i] * (geom(i, 1) * velo[3 * i + 2] - geom(i, 2) * velo[3 * i + 1]);
        angom(1) += m_mass[i] * (geom(i, 2) * velo[3 * i + 0] - geom(i, 0) * velo[3 * i + 2]);
        angom(2) += m_mass[i] * (geom(i, 0) * velo[3 * i + 1] - geom(i, 1) * velo[3 * i + 0]);
        double x2 = x * x;
        double y2 = y * y;
        double z2 = z * z;
        matrix(0, 0) += m * (y2 + z2);
        matrix(1, 1) += m * (x2 + z2);
        matrix(2, 2) += m * (x2 + y2);
        matrix(0, 1) -= m * x * y;
        matrix(0, 2) -= m * x * z;
        matrix(1, 2) -= m * y * z;
    }
    matrix(1, 0) = matrix(0, 1);
    matrix(2, 0) = matrix(0, 2);
    matrix(2, 1) = matrix(1, 2);

    Position omega = matrix.inverse() * angom;

    Position rlm = { 0, 0, 0 }, ram = { 0, 0, 0 };
    for (int i = 0; i < m_natoms; ++i) {
        rlm(0) = rlm(0) + m_mass[i] * velo[3 * i + 0];
        rlm(1) = rlm(1) + m_mass[i] * velo[3 * i + 1];
        rlm(2) = rlm(2) + m_mass[i] * velo[3 * i + 2];
    }

    for (int i = 0; i < m_natoms; ++i) {
        ram(0) = (omega(1) * geom(i, 2) - omega(2) * geom(i, 1));
        ram(1) = (omega(2) * geom(i, 0) - omega(0) * geom(i, 2));
        ram(2) = (omega(0) * geom(i, 1) - omega(1) * geom(i, 0));

        velo[3 * i + 0] = velo[3 * i + 0] - rlm(0) / mass - ram(0);
        velo[3 * i + 1] = velo[3 * i + 1] - rlm(1) / mass - ram(1);
        velo[3 * i + 2] = velo[3 * i + 2] - rlm(2) / mass - ram(2);
    }
}

void SimpleMD::PrintStatus() const
{
    auto unix_timestamp = std::chrono::seconds(std::time(NULL));

    int current = std::chrono::milliseconds(unix_timestamp).count();
    double duration = (current - m_unix_started) / (1000.0 * double(m_currentStep));
    double remaining;
    double tmp = (m_maxtime - m_currentStep) * duration / 60;
    if (tmp >= 1)
        remaining = tmp;
    else
        remaining = (m_maxtime - m_currentStep) * duration;
#pragma message("awfull, fix it ")
    if (m_writeUnique) {
#ifdef GCC
        std::cout << fmt::format("{1: ^{0}f} {2: ^{0}f} {3: ^{0}f} {4: ^{0}f} {5: ^{0}f} {6: ^{0}f} {7: ^{0}f} {8: ^{0}f} {9: ^{0}f} {10: ^{0}f} {11: ^{0}f} {12: ^{0}f} {13: ^{0}f} {14: ^{0}f} {15: ^{0}} {16: ^{0}}\n", 15,
            m_currentStep / 1000, m_Epot, m_aver_Epot, m_Ekin, m_aver_Ekin, m_Etot, m_aver_Etot, m_T, m_aver_Temp, m_wall_potential, m_average_wall_potential, m_virial_correction, m_average_virial_correction, remaining, m_time_step / 1000.0, m_unqiue->StoredStructures());
#else
        std::cout << m_currentStep * m_dT / fs2amu / 1000 << " " << m_Epot << " " << m_Ekin << " " << m_Epot + m_Ekin << m_T << std::endl;

#endif
    } else {
#ifdef GCC
        if (m_dipole)
            std::cout << fmt::format("{1: ^{0}f} {2: ^{0}f} {3: ^{0}f} {4: ^{0}f} {5: ^{0}f} {6: ^{0}f} {7: ^{0}f} {8: ^{0}f} {9: ^{0}f} {10: ^{0}f} {11: ^{0}f} {12: ^{0}f} {13: ^{0}f} {14: ^{0}f} {15: ^{0}f} {16: ^{0}f}\n", 15,
                m_currentStep / 1000, m_Epot, m_aver_Epot, m_Ekin, m_aver_Ekin, m_Etot, m_aver_Etot, m_T, m_aver_Temp, m_wall_potential, m_average_wall_potential, m_aver_dipol * 2.5418 * 3.3356, m_virial_correction, m_average_virial_correction, remaining, m_time_step / 1000.0);
        else
            std::cout << fmt::format("{1: ^{0}f} {2: ^{0}f} {3: ^{0}f} {4: ^{0}f} {5: ^{0}f} {6: ^{0}f} {7: ^{0}f} {8: ^{0}f} {9: ^{0}f} {10: ^{0}f} {11: ^{0}f} {12: ^{0}f} {13: ^{0}f} {14: ^{0}f} {15: ^{0}f}\n", 15,
                m_currentStep / 1000, m_Epot, m_aver_Epot, m_Ekin, m_aver_Ekin, m_Etot, m_aver_Etot, m_T, m_aver_Temp, m_wall_potential, m_average_wall_potential, m_virial_correction, m_average_virial_correction, remaining, m_time_step / 1000.0);
#else
        std::cout << m_currentStep * m_dT / fs2amu / 1000 << " " << m_Epot << " " << m_Ekin << " " << m_Epot + m_Ekin << m_T << std::endl;

#endif
    }
}

void SimpleMD::PrintMatrix(const double* matrix)
{
    std::cout << "Print Matrix" << std::endl;
    for (int i = 0; i < m_natoms; ++i) {
        std::cout << matrix[3 * i] << " " << matrix[3 * i + 1] << " " << matrix[3 * i + 2] << std::endl;
    }
    std::cout << std::endl;
}

double SimpleMD::CleanEnergy(double* grad)
{
    EnergyCalculator interface(m_method, m_defaults);
    interface.setMolecule(m_molecule);
    interface.updateGeometry(m_current_geometry);

    double Energy = interface.CalculateEnergy(true);
    interface.getGradient(grad);
    if (m_dipole) {
        auto dipole = interface.Dipole();
        m_curr_dipole = sqrt(dipole[0] * dipole[0] + dipole[1] * dipole[1] + dipole[2] * dipole[2]);
        m_collected_dipole.push_back(sqrt(dipole[0] * dipole[0] + dipole[1] * dipole[1] + dipole[2] * dipole[2]));
    }
    return Energy;
}

double SimpleMD::FastEnergy(double* grad)
{
    m_interface->updateGeometry(m_current_geometry);

    double Energy = m_interface->CalculateEnergy(true);
    m_interface->getGradient(grad);
    if (m_dipole) {
        auto dipole = m_interface->Dipole();
        m_curr_dipole = sqrt(dipole[0] * dipole[0] + dipole[1] * dipole[1] + dipole[2] * dipole[2]);
        m_collected_dipole.push_back(sqrt(dipole[0] * dipole[0] + dipole[1] * dipole[1] + dipole[2] * dipole[2]));
    }
    return Energy;
}

double SimpleMD::EKin()
{
    double ekin = 0;

    for (int i = 0; i < m_natoms; ++i) {
        ekin += m_mass[i] * (m_velocities[3 * i] * m_velocities[3 * i] + m_velocities[3 * i + 1] * m_velocities[3 * i + 1] + m_velocities[3 * i + 2] * m_velocities[3 * i + 2]);
    }
    ekin *= 0.5;
    m_T = 2.0 * ekin / (kb_Eh * m_dof);

    m_aver_Temp = (m_T + (m_currentStep)*m_aver_Temp) / (m_currentStep + 1);
    m_aver_Epot = (m_Epot + (m_currentStep)*m_aver_Epot) / (m_currentStep + 1);
    m_aver_Ekin = (m_Ekin + (m_currentStep)*m_aver_Ekin) / (m_currentStep + 1);
    m_aver_Etot = (m_Etot + (m_currentStep)*m_aver_Etot) / (m_currentStep + 1);
    if (m_dipole) {
        m_aver_dipol = (m_curr_dipole + (m_currentStep)*m_aver_dipol) / (m_currentStep + 1);
    }
    m_average_wall_potential = (m_wall_potential + (m_currentStep)*m_average_wall_potential) / (m_currentStep + 1);
    m_average_virial_correction = (m_virial_correction + (m_currentStep)*m_average_virial_correction) / (m_currentStep + 1);

    return ekin;
}

bool SimpleMD::WriteGeometry()
{
    bool result = true;
    Geometry geometry = m_molecule.getGeometry();
    for (int i = 0; i < m_natoms; ++i) {
        geometry(i, 0) = m_current_geometry[3 * i + 0];
        geometry(i, 1) = m_current_geometry[3 * i + 1];
        geometry(i, 2) = m_current_geometry[3 * i + 2];
    }
    TriggerWriteRestart();
    m_molecule.setGeometry(geometry);

    if (m_writeXYZ) {
        m_molecule.setEnergy(m_Epot);
        m_molecule.setName(std::to_string(m_currentStep));
        m_molecule.appendXYZFile(Basename() + ".trj.xyz");
    }
    if (m_writeUnique) {
        if (m_unqiue->CheckMolecule(new Molecule(m_molecule))) {
            std::cout << " ** new structure was added **" << std::endl;
            PrintStatus();
            m_time_step = 0;
            m_unique_structures.push_back(new Molecule(m_molecule));
        }
    }
    return result;
}

void SimpleMD::None()
{
}

void SimpleMD::Berendson()
{
    double lambda = sqrt(1 + (m_dT * (m_T0 - m_T)) / (m_T * m_coupling));
    for (int i = 0; i < 3 * m_natoms; ++i) {
        m_velocities[i] *= lambda;
    }
}

void SimpleMD::CSVR()
{
    double Ekin_target = 0.5 * kb_Eh * (m_T0)*m_dof;
    double c = exp(-(m_dT * m_respa) / m_coupling);
    static std::random_device rd{};
    static std::mt19937 gen{ rd() };
    static std::normal_distribution<> d{ 0, 1 };
    static std::chi_squared_distribution<float> dchi{ m_dof };

    double R = d(gen);
    double SNf = dchi(gen);
    double alpha2 = c + (1 - c) * (SNf + R * R) * Ekin_target / (m_dof * m_Ekin) + 2 * R * sqrt(c * (1 - c) * Ekin_target / (m_dof * m_Ekin));
    m_Ekin_exchange += m_Ekin * (alpha2 - 1);
    double alpha = sqrt(alpha2);
    for (int i = 0; i < 3 * m_natoms; ++i) {
        m_velocities[i] *= alpha;
    }
}
