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

#include "orcainterface.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>

OrcaInterface::OrcaInterface()
{
    m_inputFilePath = "orca.inp";
    m_outputFilePath = "orca.out";
}

OrcaInterface::~OrcaInterface() {
}

void OrcaInterface::setInputFile(const std::string& inputFile) {
    m_inputFilePath = inputFile;
}

bool OrcaInterface::createInputFile(const std::string& content)
{
    std::ofstream outFile(m_inputFilePath);
    if (!outFile) {
        std::cerr << "Fehler beim Erstellen der Eingabedatei!" << std::endl;
        return false;
    }
    outFile << content;
    outFile.close();
    return true;
}
std::string OrcaInterface::generateInputString()
{
    std::stringstream inputString;
    // Hinzufügen der Methode und Basis-Sets
    inputString << "! " << m_Method["method"].get<std::string>() << " "
                << m_Method["basis"].get<std::string>() << " "
                << m_Method["keyword"].get<std::string>() << "\n";

    // Hinzufügen der Dateityp-, Lade- und Multiplikationsinformationen
    inputString << "*" << m_Method["filetype"].get<std::string>() << " "
                << m_Method["charge"].get<int>() << " "
                << m_Method["mult"].get<int>() << " "
                << m_Method["basename"].get<std::string>() << ".xyz\n";

    return inputString.str();
}
void OrcaInterface::setMethod(const json& Method)
{
    m_Method = Method;
}

bool OrcaInterface::executeOrcaProcess() const {
    // Hier rufen wir das ORCA-Programm über einen Systemaufruf auf
    std::stringstream command;
    command << "orca " << m_inputFilePath << " > " << m_outputFilePath;
    int result = std::system(command.str().c_str());
    
    // Überprüfen, ob der ORCA-Prozess erfolgreich ausgeführt wurde
    return (result == 0);
}

bool OrcaInterface::runOrca() {
    // Starten Sie den ORCA-Prozess und warten Sie auf das Ergebnis
    std::cout << "Starte ORCA..." << std::endl;
    if (executeOrcaProcess()) {
        std::cout << "ORCA abgeschlossen!" << std::endl;
        return true;
    } else {
        std::cerr << "Fehler beim Ausführen von ORCA!" << std::endl;
        return false;
    }
}

void OrcaInterface::readOrcaJSON() {
    // Liest die Ergebnisse aus der ORCA-Ausgabedatei
    std::ifstream property(m_inputFilePath+".property.json");
    property >> m_OrcaJSON;
}

bool OrcaInterface::getOrcaJSON() {
    // Hier rufen wir das ORCA_2JSON-Programm über einen Systemaufruf auf
    std::stringstream command;
    command << "orca_2json " << m_inputFilePath << " -property >> " << m_outputFilePath;
    const int result = std::system(command.str().c_str());

    // Überprüfen, ob der ORCA-Prozess erfolgreich ausgeführt wurde
    return (result == 0);
}
/*
double OrcaInterface::getEnergy()
{
    return m_OrcaJSON["Geometry_1"]["DFT_Energy"]["FINALEN"];
}
Matrix OrcaInterface::getGradient()
{
    return m_OrcaJSON["Geometry_1"]["Nuclear_Gradient"]["GRAD"];
}
Vector OrcaInterface::getDipole()
{
    return m_OrcaJSON["Geometry_1"]["Dipole_Moment"]["DIPOLETOTAL"];
}
std::vector<double> OrcaInterface::getCharges()
{
    return m_OrcaJSON["Geometry_1"]["Mulliken_Population_Analysis"]["ATOMICCHARGES"];
}
std::vector<std::vector<double>>OrcaInterface::getBondOrders(){
    return m_OrcaJSON["Geometry_1"]["Mayer_Population_Analysis"]["BONDORDERS"];
}
*/