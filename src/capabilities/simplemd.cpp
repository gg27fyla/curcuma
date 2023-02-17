/*
 * <Simple MD Module for Cucuma. >
 * Copyright (C) 2020 - 2023 Conrad Hübler <Conrad.Huebler@gmx.net>
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
    m_spin = Json2KeyWord<int>(m_defaults, "spin");
    m_charge = Json2KeyWord<int>(m_defaults, "charge");
    m_timestep = Json2KeyWord<double>(m_defaults, "dT");
    m_maxtime = Json2KeyWord<double>(m_defaults, "MaxTime");
    m_T0 = Json2KeyWord<double>(m_defaults, "T");
    m_centered = Json2KeyWord<bool>(m_defaults, "centered");
    m_dumb = Json2KeyWord<int>(m_defaults, "dump");
    m_print = Json2KeyWord<int>(m_defaults, "print");
    m_max_top_diff = Json2KeyWord<int>(m_defaults, "MaxTopoDiff");

    m_rmsd = Json2KeyWord<double>(m_defaults, "rmsd");
    m_impuls = Json2KeyWord<double>(m_defaults, "impuls");
    m_impuls_scaling = Json2KeyWord<double>(m_defaults, "impuls_scaling");
    m_writeUnique = Json2KeyWord<bool>(m_defaults, "unique");
    m_opt = Json2KeyWord<bool>(m_defaults, "opt");
    m_scale_velo = Json2KeyWord<double>(m_defaults, "velo");
    m_rescue = Json2KeyWord<bool>(m_defaults, "rescue");
    m_berendson = Json2KeyWord<double>(m_defaults, "berendson");
    if (m_berendson < m_timestep)
        m_berendson = m_timestep;

    m_writeXYZ = Json2KeyWord<bool>(m_defaults, "writeXYZ");
    m_writeinit = Json2KeyWord<bool>(m_defaults, "writeinit");
    m_initfile = Json2KeyWord<std::string>(m_defaults, "initfile");
    m_norestart = Json2KeyWord<bool>(m_defaults, "norestart");
    m_dt2 = m_timestep * m_timestep;
}

bool SimpleMD::Initialise()
{
    if (!m_norestart)
        LoadRestartInformation();
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
    }
    if (m_molecule.AtomCount() == 0)
        return false;

    if (!m_restart) {
        std::ofstream result_file;
        result_file.open(m_basename + ".trj.xyz");
        result_file.close();
    }
    m_natoms = m_molecule.AtomCount();
    m_molecule.setCharge(0);

    m_mass = std::vector<double>(3 * m_natoms, 0);
    m_atomtype = std::vector<int>(m_natoms, 0);

    if (!m_restart) {
        m_current_geometry = std::vector<double>(3 * m_natoms, 0);
        m_velocities = std::vector<double>(3 * m_natoms, 0);
        m_currentStep = 0;
    }

    if(m_opt)
    {
        json js = CurcumaOptJson;
        js["writeXYZ"] = false;
        js["method"] = m_method;
        CurcumaOpt optimise(js, true);
        optimise.addMolecule(&m_molecule);
        optimise.start();
        auto mol = optimise.Molecules();
        std::cout << mol->size() << std::endl;
        auto molecule = ((*mol)[0]);
        m_molecule.setGeometry(molecule.getGeometry());
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
        } else {
            m_mass[3 * i + 0] = Elements::AtomicMass[m_atomtype[i]];
            m_mass[3 * i + 1] = Elements::AtomicMass[m_atomtype[i]];
            m_mass[3 * i + 2] = Elements::AtomicMass[m_atomtype[i]];
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
        m_unqiue->setBaseName(m_basename + ".xyz");
        m_unqiue->Initialise();
    }
    InitConstrainedBonds();
    if (m_writeinit) {
        json init = WriteRestartInformation();
        std::ofstream result_file;
        result_file.open(m_basename + ".init.json");
        result_file << init;
        result_file.close();
    }
    m_initialised = true;
    return true;
}

void SimpleMD::InitConstrainedBonds()
{
    // current just all bonds

    auto m = m_molecule.DistanceMatrix();
    m_topo_initial = m.second;
    for (int i = 0; i < m_molecule.AtomCount(); ++i) {
        for (int j = 0; j < i; ++j) {
            if (m.second(i, j))
                m_bond_constrained.push_back(std::pair<int, int>(i, j));
        }
    }
}

void SimpleMD::InitVelocities(double scaling)
{
    std::random_device rd{};
    std::mt19937 gen{ rd() };
    std::normal_distribution<> d{ 0, 1 };
    double Px = 0.0, Py = 0.0, Pz = 0.0;
    for (int i = 0; i < m_natoms; ++i) {
        double v0 = sqrt(kb * m_T0 * amu2au / (m_mass[i])) * scaling / fs2amu;
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

nlohmann::json SimpleMD::WriteRestartInformation()
{
    nlohmann::json restart;
    restart["method"] = m_method;
    restart["dT"] = m_timestep;
    restart["MaxTime"] = m_maxtime;
    restart["T"] = m_T0;
    restart["currentStep"] = m_currentStep;
    restart["velocities"] = Tools::DoubleVector2String(m_velocities);
    restart["geometry"] = Tools::DoubleVector2String(m_current_geometry);
    restart["centered"] = m_centered;
    restart["average_T"] = m_aver_Temp;
    restart["average_Epot"] = m_aver_Epot;
    restart["average_Ekin"] = m_aver_Ekin;
    restart["average_Etot"] = m_aver_Etot;
    restart["berendson"] = m_berendson;
    restart["MaxTopoDiff"] = m_max_top_diff;
    restart["impuls"] = m_impuls;
    restart["impuls_scaling"] = m_impuls_scaling;

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
    std::string geometry, velocities;

    try {
        m_method = state["method"];
    } catch (json::type_error& e) {
    }
    try {
        m_timestep = state["dT"];
    } catch (json::type_error& e) {
    }
    try {
        m_maxtime = state["MaxTime"];
    } catch (json::type_error& e) {
    }
    try {
        m_centered = state["centered"];
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
        m_berendson = state["berendson"];
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
        m_current_geometry = Tools::String2DoubleVec(geometry);
    }
    if (velocities.size()) {
        m_velocities = Tools::String2DoubleVec(velocities);
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
    double* coord = new double[3 * m_natoms];
    double* gradient = new double[3 * m_natoms];
    std::vector<json> states;
    for (int i = 0; i < 3 * m_natoms; ++i) {
        coord[i] = m_current_geometry[i];
        gradient[i] = 0;
    }

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
    m_Epot = Gradient(coord, gradient);
    m_Ekin = EKin();
    m_Etot = m_Epot + m_Ekin;

    double lambda = 1;
    int m_step = 0;
    PrintStatus();
    for (; m_currentStep <= m_maxtime;) {
        if (CheckStop() == true) {
            TriggerWriteRestart();
            return;
        }
        if (m_step % m_dumb == 0) {
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
                for (int i = 0; i < 3 * m_natoms; ++i) {
                    coord[i] = m_current_geometry[i];
                }
                Gradient(coord, gradient);
                m_Ekin = EKin();
                m_Etot = m_Epot + m_Ekin;
                m_current_rescue++;
                PrintStatus();
            }
        }
        if (m_centered)
            RemoveRotation(m_velocities);

        Verlet(coord, gradient);
        Berendson();
        m_Ekin = EKin();

        if ((m_step && m_step % m_print == 0)) {
            m_Etot = m_Epot + m_Ekin;
            PrintStatus();
            std::ofstream restart_file("curcuma_step_" + std::to_string(m_step) + ".json");
            nlohmann::json restart;
            restart_file << WriteRestartInformation() << std::endl;
        }

        if (m_impuls > m_T) {
            InitVelocities(m_scale_velo * m_impuls_scaling);
            m_Ekin = EKin();
            PrintStatus();
        }

        if (m_current_rescue >= m_max_rescue) {
            std::cout << "Nothing really helps" << std::endl;
            break;
        }
        m_step++;
        m_currentStep += m_timestep;
    }
    delete[] coord;
    delete[] gradient;
}

void SimpleMD::Verlet(double* coord, double* grad)
{
    for (int i = 0; i < m_natoms; ++i) {

        coord[3 * i + 0] = m_current_geometry[3 * i + 0] - m_timestep * m_velocities[3 * i + 0] - 0.5 * grad[3 * i + 0] / m_mass[3 * i + 0] * m_dt2;
        coord[3 * i + 1] = m_current_geometry[3 * i + 1] - m_timestep * m_velocities[3 * i + 1] - 0.5 * grad[3 * i + 1] / m_mass[3 * i + 1] * m_dt2;
        coord[3 * i + 2] = m_current_geometry[3 * i + 2] - m_timestep * m_velocities[3 * i + 2] - 0.5 * grad[3 * i + 2] / m_mass[3 * i + 2] * m_dt2;

        m_velocities[3 * i + 0] += 0.5 * m_timestep * grad[3 * i + 0] / m_mass[3 * i + 0];
        m_velocities[3 * i + 1] += 0.5 * m_timestep * grad[3 * i + 1] / m_mass[3 * i + 1];
        m_velocities[3 * i + 2] += 0.5 * m_timestep * grad[3 * i + 2] / m_mass[3 * i + 2];

        m_current_geometry[3 * i + 0] = coord[3 * i + 0];
        m_current_geometry[3 * i + 1] = coord[3 * i + 1];
        m_current_geometry[3 * i + 2] = coord[3 * i + 2];
    }
    m_Epot = Gradient(coord, grad);
    double ekin = 0.0;
    for (int i = 0; i < m_natoms; ++i) {
        m_velocities[3 * i + 0] = m_velocities[3 * i + 0] + 0.5 * m_timestep * grad[3 * i + 0] / m_mass[3 * i + 0];
        m_velocities[3 * i + 1] = m_velocities[3 * i + 1] + 0.5 * m_timestep * grad[3 * i + 1] / m_mass[3 * i + 1];
        m_velocities[3 * i + 2] = m_velocities[3 * i + 2] + 0.5 * m_timestep * grad[3 * i + 2] / m_mass[3 * i + 2];

        ekin += m_mass[i] * (m_velocities[3 * i] * m_velocities[3 * i] + m_velocities[3 * i + 1] * m_velocities[3 * i + 1] + m_velocities[3 * i + 2] * m_velocities[3 * i + 2]);
    }
    ekin *= 0.5;
    double T = 2.0 * ekin / (kb * 3 * m_natoms);
    m_unstable = T > 100 * m_T;
    m_T = T;
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
        matrix(0, 1) += m * x * y;
        matrix(0, 2) += m * x * z;
        matrix(1, 2) += m * y * z;
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
        std::cout << fmt::format("{1: ^{0}f} {2: ^{0}f} {3: ^{0}f} {4: ^{0}f} {5: ^{0}f} {6: ^{0}f} {7: ^{0}f} {8: ^{0}f} {9: ^{0}f} {10: ^{0}f} {11: ^{0}}\n", 15, m_currentStep / 1000, m_Epot, m_aver_Epot, m_Ekin, m_aver_Ekin, m_Etot, m_aver_Etot, m_T, m_aver_Temp, remaining, m_unqiue->StoredStructures());
#else
        std::cout << m_currentStep * m_timestep / fs2amu / 1000 << " " << m_Epot << " " << m_Ekin << " " << m_Epot + m_Ekin << m_T << std::endl;

#endif
    } else {
#ifdef GCC
        std::cout << fmt::format("{1: ^{0}f} {2: ^{0}f} {3: ^{0}f} {4: ^{0}f} {5: ^{0}f} {6: ^{0}f} {7: ^{0}f} {8: ^{0}f} {9: ^{0}f} {10: ^{0}f}\n", 15, m_currentStep / 1000, m_Epot, m_aver_Epot, m_Ekin, m_aver_Ekin, m_Etot, m_aver_Etot, m_T, m_aver_Temp, remaining);
#else
        std::cout << m_currentStep * m_timestep / fs2amu / 1000 << " " << m_Epot << " " << m_Ekin << " " << m_Epot + m_Ekin << m_T << std::endl;

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

double SimpleMD::Gradient(const double* coord, double* grad)
{
    m_interface->updateGeometry(coord);

    double Energy = m_interface->CalculateEnergy(true);
    m_interface->getGradient(grad);
    return Energy;
}

double SimpleMD::EKin()
{
    double ekin = 0;

    for (int i = 0; i < m_natoms; ++i) {
        ekin += m_mass[i] * (m_velocities[3 * i] * m_velocities[3 * i] + m_velocities[3 * i + 1] * m_velocities[3 * i + 1] + m_velocities[3 * i + 2] * m_velocities[3 * i + 2]);
    }
    ekin *= 0.5;
    m_T = 2.0 * ekin / (kb * 3 * m_natoms);

    m_aver_Temp = (m_T + (m_currentStep)*m_aver_Temp) / (m_currentStep + 1);
    m_aver_Epot = (m_Epot + (m_currentStep)*m_aver_Epot) / (m_currentStep + 1);
    m_aver_Ekin = (m_Ekin + (m_currentStep)*m_aver_Ekin) / (m_currentStep + 1);
    m_aver_Etot = (m_Etot + (m_currentStep)*m_aver_Etot) / (m_currentStep + 1);

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
    // int f1 = m_molecule.GetFragments().size();
    m_molecule.setGeometry(geometry);
    auto m = m_molecule.DistanceMatrix();

    // int f2 = m_molecule.GetFragments().size();
    //   std::cout << f1 << " ... " << f2 << std::endl;
    // m_prev_index = std::abs(f2 - f1);
    /*
    int difference = (m.second - m_topo_initial).cwiseAbs().sum();

    if (difference > m_max_top_diff) {
        std::cout << "*** topology changed " << difference << " ***" << std::endl;
        result = false;
    }
    */
    if (m_writeXYZ) {
        m_molecule.setEnergy(m_Epot);
        m_molecule.setName(std::to_string(m_currentStep));
        m_molecule.appendXYZFile(m_basename + ".trj.xyz");
    }
    if (m_writeUnique) {
        if (m_unqiue->CheckMolecule(new Molecule(m_molecule))) {
            std::cout << " ** new structure was added **" << std::endl;
            PrintStatus();
            m_unique_structures.push_back(new Molecule(m_molecule));
        }
    }
    return result;
}

void SimpleMD::Berendson()
{
    double lambda = sqrt(1 + (m_timestep * (m_T0 - m_T)) / (m_T * m_berendson));
    for (int i = 0; i < 3 * m_natoms; ++i) {
        m_velocities[i] *= lambda;
    }
}
