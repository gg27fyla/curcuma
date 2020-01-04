/*
 * <RMSD calculator for chemical structures.>
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

#pragma once

#include "src/core/molecule.h"
#include "src/core/global.h"

#include <map>
#include <queue>

class IntermediateStorage {
public:
    inline IntermediateStorage(unsigned int size)
        : m_size(size)
    {
    }

    inline void addItem(const std::vector<int>& vector, double rmsd)
    {

        m_shelf.insert(std::pair<double, std::vector<int>>(rmsd, vector));
        if (m_shelf.size() >= m_size)
            m_shelf.erase(--m_shelf.end());
    }

    const std::map<double, std::vector<int>>* data() const { return &m_shelf; }

private:
    unsigned int m_size;
    std::map<double, std::vector<int>> m_shelf;
};

class RMSDDriver{

public:
    RMSDDriver(const Molecule& reference, const Molecule& target);

    /*! \brief Use the AutoPilot to automatically perform everything, results are stored as long the object exsist */
    void AutoPilot();

    double CalculateRMSD();
    double CalculateRMSD(const Molecule& reference, const Molecule& target, Molecule* ret_ref = nullptr, Molecule* ret_tar = nullptr, int factor = 1) const;
    std::vector<double> IndivRMSD(const Molecule& reference, const Molecule& target, int factor = 1) const;

    void ReorderMolecule();

    /*! \brief Return the reference molecule centered */
    inline Molecule ReferenceAligned() const { return m_reference_aligned; }

    /*! \brief Return the target molecule centered and aligned to the reference molecule */
    inline Molecule TargetAligned() const { return m_target_aligned; }

    /*! \brief Return the target molecule reorderd but remaining at the original position */
    inline Molecule TargetReorderd() const { return m_target_reordered; }

    /*! \brief Return best-fit reordered RMSD */
    inline double RMSD() const { return m_rmsd; }

    /*! \brief Return best-fit RMSD with reordering */
    inline double RMSDRaw() const { return m_rmsd_raw; }

    /*! \brief Force Reordering, even the sequence of elements are equal */
    inline void setForceReorder(bool reorder) { m_force_reorder = reorder; }

    /*! \brief Check, if Reordering is force */
    inline bool ForceReorder() const { return m_force_reorder; }

    /*! \brief Get n'th/rd best fit result */
    Molecule getFitIndex(int index);

private:
    void SolveIntermediate(std::vector<int> intermediate);
    Eigen::Matrix3d BestFitRotation(const Molecule& reference, const Molecule& target, int factor = 1) const;
    Geometry CenterMolecule(const Molecule &mol) const;

    Molecule m_reference, m_target, m_reference_aligned, m_target_aligned, m_target_reordered;
    bool m_force_reorder = false;
    std::queue<std::vector<int>> m_intermediate_results;
    std::map<double, std::vector<int>> m_results;
    std::vector<IntermediateStorage> m_storage;
    double m_rmsd = 0, m_rmsd_raw = 0;
    int m_hit = 1;
};