/*
 * <Collection of functions to calculate rmsd. >
 * Copyright (C) 2020 Conrad Hübler <Conrad.Huebler@gmx.net>
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

#include "src/core/global.h"
#include "src/core/molecule.h"

#include "src/tools/geometry.h"

#include <Eigen/Dense>

namespace RMSDFunctions {

/*! \brief Calculate the best fit rotation of two sets of coordinates, both have to be centered already */

inline Eigen::Matrix3d BestFitRotation(const Geometry& reference, const Geometry& target, int factor = 1)
{
    /* The rmsd kabsch algorithmn was adopted from here:
     * https://github.com/oleg-alexandrov/projects/blob/master/eigen/Kabsch.cpp
     * The specific git commit was
     * https://github.com/oleg-alexandrov/projects/blob/e7b1eb7a4d83d41af563c24859072e4ddd9b730b/eigen/Kabsch.cpp
     */

    //   std::cout << reference << std::endl << target << std::endl;
    Eigen::MatrixXd Cov = reference.transpose() * target;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(Cov, Eigen::ComputeThinU | Eigen::ComputeThinV);

    double d = (svd.matrixV() * svd.matrixU().transpose()).determinant();
    if (d > 0)
        d = factor * 1.0;
    else
        d = factor * -1.0;
    Eigen::Matrix3d I = Eigen::Matrix3d::Identity(3, 3);
    I(2, 2) = d;
    //    std::cout <<  svd.matrixV() * I * svd.matrixU().transpose() << std::endl;

    return svd.matrixV() * I * svd.matrixU().transpose();
}

inline Eigen::Matrix3d BestFitRotation(const Molecule& reference, const Molecule& target, int factor = 1)
{
    return BestFitRotation(reference.getGeometry(), target.getGeometry(), factor);
}

inline Geometry applyRotation(const Geometry& geometry, const Eigen::Matrix3d& rotation)
{
    return geometry * rotation;
}

inline Geometry getAligned(const Geometry& reference, const Geometry& target, int factor)
{
    Eigen::Matrix3d rotation = BestFitRotation(reference, target, factor);
    return applyRotation(target, rotation);
}

inline Molecule getAligned(const Molecule& reference, const Molecule& target, int factor)
{
    Molecule result = target;
    result.setGeometry(getAligned(reference.getGeometry(), target.getGeometry(), factor));
    return result;
}

inline double getRMSD(const Geometry& reference, const Geometry& target)
{
    double rmsd = 0.0;
    for (int i = 0; i < target.rows(); ++i) {
        rmsd += (target(i, 0) - reference(i, 0)) * (target(i, 0) - reference(i, 0))
            + (target(i, 1) - reference(i, 1)) * (target(i, 1) - reference(i, 1))
            + (target(i, 2) - reference(i, 2)) * (target(i, 2) - reference(i, 2));
    }
    rmsd = sqrt(rmsd / double(target.rows()));
    return rmsd;
}
};
