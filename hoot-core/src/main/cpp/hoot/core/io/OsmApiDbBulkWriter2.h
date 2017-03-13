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
 * @copyright Copyright (C) 2016, 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef OSMAPIDBBULKWRITER2_H
#define OSMAPIDBBULKWRITER2_H

#include <hoot/core/io/OsmApiDbBulkWriter.h>

namespace hoot
{

using namespace boost;
using namespace std;

class OsmApiDbCsvTableFileSetWriter;

/**
 * Version of osm api db bulk writing intended to utilize the pg_bulkload utility.
 */
class OsmApiDbBulkWriter2 : public OsmApiDbBulkWriter
{

public:

  static string className() { return "hoot::OsmApiDbBulkWriter2"; }

  static unsigned int logWarnCount;

  OsmApiDbBulkWriter2();

  virtual ~OsmApiDbBulkWriter2();

  virtual bool isSupported(QString url);

  virtual void finalizePartial();

  virtual void setConfiguration(const Settings& conf);

  void setDisableWriteAheadLogging(bool disable) { _disableWriteAheadLogging = disable; }
  void setWriteMultithreaded(bool multithreaded) { _writeMultiThreaded = multithreaded; }

protected:

  virtual void _retainOutputFiles();
  virtual void _writeDataToDb();

private:

  bool _disableWriteAheadLogging;
  bool _writeMultiThreaded;
};

}

#endif // OSMAPIDBBULKWRITER2_H
