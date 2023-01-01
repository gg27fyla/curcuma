/*
 * < General Energy and Gradient Calculator >
 * Copyright (C) 2022 - 2023 Conrad Hübler <Conrad.Huebler@gmx.net>
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

#pragma once

#include "src/tools/general.h"

#ifdef USE_TBLITE
#include "src/core/tbliteinterface.h"
#endif

#ifdef USE_XTB
#include "src/core/xtbinterface.h"
#endif

#include "src/core/uff.h"

#include <functional>

class EnergyCalculator {
public:
    EnergyCalculator(const std::string& method, const json& controller);
    ~EnergyCalculator();

    void setMolecule(const Molecule& molecule);

    void updateGeometry(const double* coord);
    void updateGeometry(const std::vector<double>& geometry);
    void updateGeometry(const Eigen::VectorXd& geometry);

    void getGradient(double* coord);
    std::vector<std::array<double, 3>> getGradient() const { return m_gradient; }

    double CalculateEnergy(bool gradient = false);

private:
    void InitialiseUFF();
    void CalculateUFF(bool gradient);

    void InitialiseTBlite();
    void CalculateTBlite(bool gradient);

    void InitialiseXTB();
    void CalculateXTB(bool gradient);

    json m_controller;

#ifdef USE_TBLITE
    TBLiteInterface* m_tblite;
#endif

#ifdef USE_XTB
    XTBInterface* m_xtb;
#endif

    UFF* m_uff;
    StringList m_uff_methods = { "uff" };
    StringList m_tblite_methods = { "gfn1", "gfn2" };
    StringList m_xtb_methods = { "gfnff", "xtb-gfn1", "xtb-gfn2" };

    std::function<void(bool)> m_ecengine;
    std::string m_method;
    std::vector<std::array<double, 3>> m_geometry, m_gradient;
    Matrix m_eigen_geometry, m_eigen_gradient;
    double m_energy;
    double *m_coord, *m_grad;

    int m_atoms;
    int m_gfn = 2;
    int* m_atom_type;

    bool m_initialised = false;
};