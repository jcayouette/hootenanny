/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
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
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2005 VividSolutions (http://www.vividsolutions.com/)
 * @copyright Copyright (C) 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "EarthMoverDistanceExtractor.h"

// geos
#include <geos/geom/Geometry.h>
#include <geos/util/TopologyException.h>

// hoot
#include <hoot/core/Factory.h>
#include <hoot/core/util/GeometryUtils.h>
#include <hoot/core/elements/ElementVisitor.h>
#include <hoot/core/visitors/AngleHistogramVisitor.h>
#include <hoot/core/algorithms/WayHeading.h>

namespace hoot
{

HOOT_FACTORY_REGISTER(FeatureExtractor, EarthMoverDistanceExtractor)

EarthMoverDistanceExtractor::EarthMoverDistanceExtractor()
{
}

Mat EarthMoverDistanceExtractor::dist(const Mat sig1, const Mat sig2) const
{
  Mat cost(sig1.rows, sig2.rows, CV_32FC1);
  for (int i = 0; i < sig1.rows; i++)
  {
    for (int j = 0; j < sig2.rows; j++)
    {
      double delta = WayHeading::deltaMagnitude(sig1.at<float>(i,0), sig2.at<float>(i,0));
      cost.at<float>(i,j) = delta;
    }
  }
  return cost;
}

Mat EarthMoverDistanceExtractor::_createMat(const OsmMap& map, const ConstElementPtr& e) const
{
  Histogram* his = new Histogram(16);
  AngleHistogramVisitor v(*his, map);
  e->visitRo(map, v);

  vector<double> bins = his->getBins();
  //his->normalize();
  //vector<double> norm = his->getBins();
  Mat mat(bins.size(), 1, CV_32FC1);
  for (unsigned int i = 0; i < bins.size(); i++)
  {
    mat.at<float>(i, 0) = bins[i];
    //mat.at<float>(i, 1) = norm[i];
  }
  return mat;
}

double EarthMoverDistanceExtractor::extract(const OsmMap& map, const ConstElementPtr& target,
  const ConstElementPtr& candidate) const
{
  //make signature
  Mat sig1 = _createMat(map, target);
  Mat sig2 = _createMat(map, candidate);

  //compare similarity of 2D using emd. emd 0 is best matching.
  Mat cost = dist(sig1, sig2);
  double emd = cv::EMD(sig1, sig2, CV_DIST_USER, cost);
  LOG_VAR(emd);
  return emd;
}

}
