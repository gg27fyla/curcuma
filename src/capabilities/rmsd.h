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

#include <chrono>
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
    RMSDDriver(const Molecule* reference, const Molecule* target);
    RMSDDriver() = default;

    ~RMSDDriver();
    /*! \brief Use the AutoPilot to automatically perform everything, results are stored as long the object exsist */
    void AutoPilot();

    inline void setReference(const Molecule& reference) { m_reference = reference; }
    inline void setTarget(const Molecule& target) { m_target = target; }

    double Rules2RMSD(const std::vector<int> rules);

    double CalculateRMSD();
    double CalculateRMSD(const Molecule& reference, const Molecule& target, Molecule* ret_ref = nullptr, Molecule* ret_tar = nullptr, int factor = 1) const;

    void ProtonDepleted();

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

    /*! \brief Check, if Reordering is forced */
    inline bool ForceReorder() const { return m_force_reorder; }

    /*! \brief Get n'th/rd best fit result */
    Molecule getFitIndex(int index);

    /*! \brief Set the index of the fragment that is used for rmsd calculation/atom reordering */
    inline void setFragment(int fragment)
    {
        m_fragment = fragment;
        m_fragment_reference = fragment;
        m_fragment_target = fragment;
    }

    /*! \brief Set the index of the fragment that is used for rmsd calculation/atom reordering */
    inline void setFragmentTarget(int fragment) { m_fragment_target = fragment; }

    /*! \brief Set the index of the fragment that is used for rmsd calculation/atom reordering */
    inline void setFragmentReference(int fragment) { m_fragment_reference = fragment; }

    /*! \brief Set to use protons (true = default or false) */
    inline void setProtons(bool protons) { m_protons = protons; }

    /*! \brief Set Connectivitiy Check forced (true or false = default) */
    inline void setCheckConnections(bool check) { m_check_connections = check; }

    /*! \brief Force Connectivitiy Check */
    inline bool CheckConnections() const { return m_check_connections; }

    /*! \brief Number of Proton changes allowed */
    inline int ProtonTransfer() const { return m_pt; }

    /*! \brief Set number of allowed proton transfer */
    inline void setProtonTransfer(int pt) { m_pt = pt; }

    /*! \brief Set silent */
    inline void setSilent(bool silent) { m_silent = silent; }

    /*! \brief Set silent */
    inline void setPartialRMSD(bool partial_rmsd) { m_partial_rmsd = partial_rmsd; }

    void setScaling(double scaling) { m_scaling = scaling; }

    inline void setIntermediateStorage(double storage) { m_intermedia_storage = storage; }

    inline std::vector<int> ReorderRules() const { return m_reorder_rules; }

    inline void setInitial(std::vector<int> initial) { m_initial = initial; }
    inline void setInitialFragment(int fragment) { m_initial_fragment = fragment; }

private:
    void ReorderStraight();
    void ReconstructTarget(const std::vector<int>& atoms);

    void InitialiseOrder();
    void InitialisePair();

    bool SolveIntermediate(std::vector<int> intermediate, bool fast = false);

    int CheckConnectivitiy(const Molecule& mol1, const Molecule& mol2) const;
    int CheckConnectivitiy(const Molecule& mol1) const;

    void clear();

    Eigen::Matrix3d BestFitRotation(const Molecule& reference, const Molecule& target, int factor = 1) const;
    Eigen::Matrix3d BestFitRotation(const Geometry& reference, const Geometry& target, int factor = 1) const;

    double CalculateShortRMSD(const Geometry& reference_mol, const Molecule& target_mol) const;
    Eigen::Matrix3d BestFitRotationShort(const Geometry& reference, const Geometry& target) const;

    Geometry CenterMolecule(const Molecule& mol, int fragment) const;
    Geometry CenterMolecule(const Geometry& molt) const;

    Molecule m_reference, m_target, m_reference_aligned, m_target_aligned, m_target_reordered;
    bool m_force_reorder = false, m_protons = true, m_print_intermediate = false, m_silent = false;
    std::queue<std::vector<int>> m_intermediate_results;
    std::map<double, std::vector<int>> m_results;
    std::vector<double> m_last_rmsd;
    std::vector<int> m_reorder_rules;
    std::map<int, std::vector<int>> m_connectivity;
    std::vector<IntermediateStorage> m_storage;
    double m_rmsd = 0, m_rmsd_raw = 0, m_scaling = 1.5, m_intermedia_storage = 1, m_threshold = 99;
    bool m_check_connections = false, m_partial_rmsd = false, m_postprocess = true;
    int m_hit = 1, m_pt = 0, m_reference_reordered = 0, m_heavy_init = 0, m_init_count = 0, m_initial_fragment = -1;
    mutable int m_fragment = -1, m_fragment_reference = -1, m_fragment_target = -1;
    std::vector<int> m_initial;
};
