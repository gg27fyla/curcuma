/*
 * < C++ XTB and tblite Interface >
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

#pragma once

#include "src/tools/general.h"

#ifndef tblite_delete
#include "tblite.h"
#endif

#include "src/core/molecule.h"

static json TBLiteSettings{
    { "tb_acc", 1 },
    { "tb_max_iter", 250 },
    { "tb_damping", 0.4 },
    { "tb_temp", 9.500e-4 },
    { "tb_verbose", 0 },
    { "tb_guess", "SAD" },
    { "cpcm_solv", "none" },
    { "alpb_solv", "none" },
    { "cpcm_eps", -1 },
    { "alpb_eps", -1 }
};

class UFF;

class TBLiteInterface {
public:
    TBLiteInterface(const json& tblitesettings = TBLiteSettings);
    ~TBLiteInterface();

    bool InitialiseMolecule(const Molecule& molecule);
    bool InitialiseMolecule(const Molecule* molecule);
    bool InitialiseMolecule(const int* attyp, const double* coord, const int natoms, const double charge, const int spin);

    bool UpdateMolecule(const Molecule& molecule);
    bool UpdateMolecule(const double* coord);

    bool Error() { return m_error_count >= 10; }
    double GFNCalculation(int parameter = 2, double* grad = nullptr);

    void clear();

    std::vector<double> Charges() const;
    std::vector<double> Dipole() const;

    std::vector<std::vector<double>> BondOrders() const;

private:
    void ApplySolvation();

    void tbliteError();
    void tbliteContextError();
    Molecule m_molecule;
    double* m_coord;
    int* m_attyp;

    int m_atomcount = 0;
    double m_thr = 1.0e-10;
    int m_acc = 2;
    int m_maxiter = 100;
    int m_verbose = 0;
    int m_guess = 0;
    int m_error_count = 0;
    double m_damping = 0.5;
    double m_temp = 1000;
    double m_cpcm_eps = -1, m_alpb_eps = -1;
    char *m_cpcm_solv = "none", *m_alpb_solv = "none";
    bool m_cpcm = false, m_alpb = false;
    tblite_error m_error = NULL;
    tblite_structure m_tblite_mol = NULL;
    tblite_result m_tblite_res = NULL;
    tblite_context m_ctx = NULL;
    tblite_calculator m_tblite_calc = NULL;
    tblite_container m_tb_cont = NULL;

    bool m_initialised = false, m_calculator = false;
    json m_tblitesettings;
};
