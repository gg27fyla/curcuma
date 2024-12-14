/*
* <Curcuma main file.>
 * Copyright (C) 2019 - 2023 Conrad Hübler <Conrad.Huebler@gmx.net>
 *               2024 Gerd Gehrisch <gg27fyla@student.freiberg.de>
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

#include "src/core/molecule.h"

static json method{
        { "methode", "HF" },
        { "basis", "def2-TZVPP" },
        { "keyword", "ENGRAD" },
        { "filetype", "xyzfile" },
        { "charge", 0 },
        { "mult", 1 },
        { "basename", "input" },
};

static std::string InputString = "! HF def2-TZYPP ENGRAD\n*xyzfile 0 1 input.xyz";



class OrcaInterface {
public:
    explicit OrcaInterface();
    ~OrcaInterface();

    // Setzt die ORCA Eingabedaten
    void setInputFile(const std::string& inputFile);

    bool createInputFile(const std::string& content=InputString);
    void setMethod(const json& Method);

    // Führt ORCA aus und wartet auf die Beendigung
    bool runOrca();

    double getEnergy();
    Matrix getGradient();
    Vector getDipole();
    std::vector<double> getCharges();
    std::vector<std::vector<double>> getBondOrders();

private:
    std::string m_inputFilePath;
    std::string m_outputFilePath;
    std::string m_inputString = InputString;
    json m_Method = method;
    json m_OrcaJSON;

    // Hilfsfunktionen

    // Liest die Ergebnisse aus der ORCA-Ausgabedatei
    void readOrcaJSON();

    // Create Output.JSON
    bool getOrcaJSON();

    bool executeOrcaProcess() const;

    std::string generateInputString();
};
