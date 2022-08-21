/*
 * < C++ XTB and tblite Interface >
 * Copyright (C) 2020 - 2022 Conrad Hübler <Conrad.Huebler@gmx.net>
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

#ifdef USE_XTB
#include "external/tblite/include/tblite.h"
#include "external/xtb/include/xtb.h"
#endif

#include "src/core/molecule.h"

#include "src/tools/general.h"

static json xTBSettings{
    { "calculator_accuracy", 2 },
    { "calculator_max_iter", 100 }
};

class TBLiteInterface {
public:
    TBLiteInterface(const json& xtbsettings = xTBSettings);
    ~TBLiteInterface();

    bool InitialiseMolecule(const Molecule& molecule);
    bool InitialiseMolecule(const Molecule* molecule);
    bool InitialiseMolecule(const int* attyp, const double* coord, const int natoms, const double charge, const int spin);

    bool UpdateMolecule(const Molecule& molecule);
    bool UpdateMolecule(const double* coord);

    /* int parameter
     * 66 = xtb GFN FF
     * 0 = xtb GFN 0
     * 1 = xtb GFN 1
     * 2 = xtb GFN 2
     * */
    double GFNCalculation(int parameter = 2, double* grad = 0);

    void clear();

private:
#ifdef USE_XTB
    double m_thr = 1.0e-10;
    // tblite stuff
    tblite_error m_error = NULL;
    tblite_structure m_tblite_mol = NULL;
    tblite_result m_tblite_res = NULL;
    tblite_context m_ctx = NULL;
    tblite_calculator m_tblite_calc = NULL;

    // xtb stuff
    xtb_TEnvironment m_env = NULL;
    xtb_TMolecule m_xtb_mol = NULL;
    xtb_TCalculator m_xtb_calc = NULL;
    xtb_TResults m_xtb_res = NULL;

    // tblite_container cont = NULL;
#endif
    bool m_initialised = false;
    json m_xtbsettings;
};