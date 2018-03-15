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
 * @copyright Copyright (C) 2015, 2016, 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "OsmGbdxXmlWriter.h"

// Boost
using namespace boost;

// Hoot
#include <hoot/core/OsmMap.h>
#include <hoot/core/elements/Node.h>
#include <hoot/core/elements/Relation.h>
#include <hoot/core/elements/Tags.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/filters/NoInformationCriterion.h>
#include <hoot/core/index/OsmMapIndex.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Exception.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/util/MetadataTags.h>
#include <hoot/core/util/OsmUtils.h>
#include <hoot/core/util/UuidHelper.h>
#include <hoot/core/visitors/CalculateMapBoundsVisitor.h>

// Qt
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QXmlStreamWriter>

using namespace geos::geom;
using namespace std;

namespace hoot
{

unsigned int OsmGbdxXmlWriter::logWarnCount = 0;

HOOT_FACTORY_REGISTER(OsmMapWriter, OsmGbdxXmlWriter)

OsmGbdxXmlWriter::OsmGbdxXmlWriter() :
_formatXml(ConfigOptions().getOsmMapWriterFormatXml()),
_includeIds(false),
_includeDebug(ConfigOptions().getWriterIncludeDebugTags()),
_includePointInWays(false),
_textStatus(ConfigOptions().getWriterTextStatus()),
_timestamp("1970-01-01T00:00:00Z"),
_precision(ConfigOptions().getWriterPrecision()),
_encodingErrorCount(0),
_includeCircularErrorTags(ConfigOptions().getWriterIncludeCircularErrorTags())
{
}

OsmGbdxXmlWriter::~OsmGbdxXmlWriter()
{
  close();
}

QString OsmGbdxXmlWriter::removeInvalidCharacters(const QString& s)
{
  // See #3553 for an explanation.

  QString result;
  result.reserve(s.size());

  bool foundError = false;
  for (int i = 0; i < s.size(); i++)
  {
    QChar c = s[i];
    // See http://stackoverflow.com/questions/730133/invalid-characters-in-xml
    if (c < 0x20 && c != 0x9 && c != 0xA && c != 0xD)
    {
      foundError = true;
    }
    else
    {
      result.append(c);
    }
  }

  if (foundError)
  {
    _encodingErrorCount++;
    if (logWarnCount < Log::getWarnMessageLimit())
    {
      LOG_WARN("Found an invalid character in string: '" << s << "'");
      LOG_WARN("  UCS-4 version of the string: " << s.toUcs4());
    }
    else if (logWarnCount == Log::getWarnMessageLimit())
    {
      LOG_WARN(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
    }
    logWarnCount++;
  }

  return result;
}

void OsmGbdxXmlWriter::open(QString url)
{
    LOG_INFO("Starting Open");
    if (url.toLower().endsWith(".gxml"))
    {
      url.remove(url.size() - 5, url.size());
    }

    _outputDir = QDir(url);
    _outputDir.makeAbsolute();

    if (_outputDir.exists() == false)
    {
      if (QDir().mkpath(_outputDir.path()) == false)
      {
        throw HootException("Error creating directory for writing.");
      }
    }
  _bounds.init();
}

void OsmGbdxXmlWriter::_newOutputFile()
{
  // Close the old file and open a new one
  if (_fp.get())
  {
    // Calling this so that the XML gets closed
    close();
  }

  QString url = _outputDir.filePath(UuidHelper::createUuid().toString().replace("{", "").replace("}", "") + ".xml");

  QFile* f = new QFile();
  _fp.reset(f);
  f->setFileName(url);

  LOG_VARI(url);

  if (!_fp->open(QIODevice::WriteOnly | QIODevice::Text))
  {
    throw Exception(QObject::tr("Error opening %1 for writing").arg(url));
  }

  _writer.reset(new QXmlStreamWriter(_fp.get()));
  _writer->setCodec("UTF-8");

  if (_formatXml)
  {
    _writer->setAutoFormatting(true);
  }

  _writer->writeStartDocument();
  _writer->writeStartElement("metadata");

}

void OsmGbdxXmlWriter::close()
{
  if (_writer.get())
  {
    _writer->writeEndElement();
    _writer->writeEndDocument();
  }

  if (_fp.get())
  {
    _fp->close();
  }
}

QString OsmGbdxXmlWriter::toString(const ConstOsmMapPtr& map, const bool formatXml)
{
  OsmGbdxXmlWriter writer;
  writer.setFormatXml(formatXml);
  // this will be deleted by the _fp boost::shared_ptr
  QBuffer* buf = new QBuffer();
  writer._fp.reset(buf);
  if (!writer._fp->open(QIODevice::WriteOnly | QIODevice::Text))
  {
    throw InternalErrorException(QObject::tr("Error opening QBuffer for writing. Odd."));
  }
  writer.write(map);
  return QString::fromUtf8(buf->data(), buf->size());
}

QString OsmGbdxXmlWriter::_typeName(ElementType e)
{
  switch(e.getEnum())
  {
  case ElementType::Node:
    return "node";
  case ElementType::Way:
    return "way";
  case ElementType::Relation:
    return "relation";
  default:
    throw HootException("Unexpected element type.");
  }
}

void OsmGbdxXmlWriter::write(ConstOsmMapPtr map, const QString& path)
{
  open(path);
  write(map);
}

void OsmGbdxXmlWriter::write(ConstOsmMapPtr map)
{
  //Some code paths don't call the open method before invoking this write method, so make sure the
  //writer has been initialized.
//  if (!_writer.get())
//  {
//      LOG_INFO("No Writer. Calling _newOutputFile");
//    _newOutputFile();
//  }

  //TODO: The coord sys and schema entries don't get written to streamed output b/c we don't have
  //the map object to read the coord sys from.

//  int epsg = map->getProjection()->GetEPSGGeogCS();
//  if (epsg > -1)
//  {
//    _writer->writeAttribute("CRS", QString("%1").arg(epsg));
//  }
//  else
//  {
//    char *wkt;
//    map->getProjection()->exportToWkt(&wkt);
//    _writer->writeAttribute("CRS", wkt);
//    free(wkt);
//  }

  // Thought: Grab a polygon/way, copy it to a map and then call get bounds
//  const geos::geom::Envelope bounds = CalculateMapBoundsVisitor::getGeosBounds(map);
//  _writeBounds(bounds);

//  const Envelope& bounds = way->getEnvelopeInternal(_map);
//  way->setTag("hoot:bbox",QString("%1,%2,%3,%4").arg(QString::number(bounds.getMinX(), 'g', 10))
//              .arg(QString::number(bounds.getMinY(), 'g', 10))
//              .arg(QString::number(bounds.getMaxX(), 'g', 10))
//              .arg(QString::number(bounds.getMaxY(), 'g', 10)));

  _writeNodes(map);
  _writeWays(map);
  _writeRelations(map);

  close();
}

//void OsmGbdxXmlWriter::_writeMetadata(const Element *e)
//{
//  //TODO: This comparison seems to be still unequal when I set an element's timestamp to
//  //ElementData::TIMESTAMP_EMPTY.  See RemoveAttributeVisitor
//  if (e->getTimestamp() != ElementData::TIMESTAMP_EMPTY)
//  {
//    _writer->writeAttribute("timestamp", OsmUtils::toTimeString(e->getTimestamp()));
//  }
//  if (e->getVersion() != ElementData::VERSION_EMPTY)
//  {
//    _writer->writeAttribute("version", QString::number(e->getVersion()));
//  }
//  if (e->getChangeset() != ElementData::CHANGESET_EMPTY)
//  {
//    _writer->writeAttribute("changeset", QString::number(e->getChangeset()));
//  }
//  if (e->getUser() != ElementData::USER_EMPTY)
//  {
//    _writer->writeAttribute("user", e->getUser());
//  }
//  if (e->getUid() != ElementData::UID_EMPTY)
//  {
//    _writer->writeAttribute("uid", QString::number(e->getUid()));
//  }
//}

void OsmGbdxXmlWriter::_writeTags(const ConstElementPtr& element)
{

  // GBDX XML format:
  //   <M_Lang>English</M_Lang>

  const ElementType type = element->getElementType();
  assert(type != ElementType::Unknown);
  const Tags& tags = element->getTags();

  for (Tags::const_iterator it = tags.constBegin(); it != tags.constEnd(); ++it)
  {
    const QString key = it.key();
    const QString val = it.value().trimmed();
    if (val.isEmpty() == false)
    {
      _writer->writeStartElement(removeInvalidCharacters(key));
      _writer->writeCharacters(removeInvalidCharacters(val));
      _writer->writeEndElement();
    }
  }

  if (type == ElementType::Relation)
  {
    ConstRelationPtr relation = boost::dynamic_pointer_cast<const Relation>(element);
    if (relation->getType() != "")
    {
      _writer->writeStartElement("Relation");
      _writer->writeAttribute("k", "type");
      _writer->writeAttribute("v", removeInvalidCharacters(relation->getType()));
      _writer->writeEndElement();
    }
  }

}

void OsmGbdxXmlWriter::_writeNodes(ConstOsmMapPtr map)
{
  NoInformationCriterion filter;
  QList<long> nids;
  const NodeMap& nodes = map->getNodes();
  for (NodeMap::const_iterator it = nodes.begin(); it != nodes.end(); ++it)
  {
    if (filter.isNotSatisfied(map->getNode(it->first)))
        nids.append(it->first);
  }

  // sort the values to give consistent results.
  qSort(nids.begin(), nids.end(), qLess<long>());
  for (int i = 0; i < nids.size(); i++)
  {
    _newOutputFile();
    writePartial(map->getNode(nids[i]));
  }
}

void OsmGbdxXmlWriter::_writeWays(ConstOsmMapPtr map)
{
  const WayMap& ways = map->getWays();

  for (WayMap::const_iterator it = ways.begin(); it != ways.end(); ++it)
  {
    ConstWayPtr w = it->second;
    //  Skip any ways that have parents
    set<ElementId> parents = map->getParents(w->getElementId());
    if (parents.size() > 0)
      continue;
    if (w.get() == NULL)
      continue;

    //  Make sure that building ways are "complete"
    const vector<long>& nodes = w->getNodeIds();
    bool valid = true;
    if (OsmSchema::getInstance().isArea(w))
    {
      for (vector<long>::const_iterator nodeIt = nodes.begin(); nodeIt != nodes.end(); ++nodeIt)
      {
        ConstNodePtr node = map->getNode(*nodeIt);
        if (node.get() == NULL)
        {
          valid = false;
          break;
        }
      }
    }
    //  Write out the way in GbdxXml if valid
    if (valid)
    {
      _newOutputFile();
      _writePartialIncludePoints(w,map);
    }
    else
    {
      for (vector<long>::const_iterator nodeIt = nodes.begin(); nodeIt != nodes.end(); ++nodeIt)
      {
        ConstNodePtr node = map->getNode(*nodeIt);
        if (node.get() != NULL)
        {
          LOG_INFO("Writing Nodes XXX");
          _newOutputFile();
          writePartial(node);
        }
      }
    }
  }

}

void OsmGbdxXmlWriter::_writeRelations(ConstOsmMapPtr map)
{
  QList<long> rids;
  const RelationMap& relations = map->getRelations();
  for (RelationMap::const_iterator it = relations.begin(); it != relations.end(); ++it)
  {
    rids.append(it->first);
  }

  // sort the values to give consistent results.
  qSort(rids.begin(), rids.end(), qLess<long>());
  for (int i = 0; i < rids.size(); i++)
  {
    _newOutputFile();
    writePartial(map->getRelation(rids[i]));
  }
}

void OsmGbdxXmlWriter::_writeBounds(const Envelope& bounds)
{
  _writer->writeStartElement("bounds");
  _writer->writeAttribute("minlat", QString::number(bounds.getMinY(), 'g', _precision));
  _writer->writeAttribute("minlon", QString::number(bounds.getMinX(), 'g', _precision));
  _writer->writeAttribute("maxlat", QString::number(bounds.getMaxY(), 'g', _precision));
  _writer->writeAttribute("maxlon", QString::number(bounds.getMaxX(), 'g', _precision));
  _writer->writeEndElement();
}

void OsmGbdxXmlWriter::writePartial(const ConstNodePtr& n)
{
  LOG_VART(n);

  _writer->writeStartElement("Location");
  _writer->writeCharacters(QString("POINT (%1 %2)").arg(QString::number(n->getX(), 'f', _precision)).arg(QString::number(n->getY(), 'f', _precision)));
  _writer->writeEndElement();

  _writeTags(n);
}

void OsmGbdxXmlWriter::_writePartialIncludePoints(const ConstWayPtr& w, ConstOsmMapPtr map)
{
  LOG_VART(w);

  _writer->writeStartElement("Location");

  const vector<long>& nodes = w->getNodeIds();

  if (OsmSchema::getInstance().isArea(w) || nodes[0] == nodes[nodes.size() - 1])
  {
    _writer->writeCharacters(QString("POLYGON ("));
  }
  else
  {
    _writer->writeCharacters(QString("LINESTRING ("));
  }

  bool first = true;
  for (size_t j = 0; j < w->getNodeCount(); j++)
  {
    if (first)
      first = false;
    else
      _writer->writeCharacters(QString(", "));

    const long nid = w->getNodeId(j);
    ConstNodePtr n = map->getNode(nid);
    _writer->writeCharacters(QString("%1 %2").arg(QString::number(n->getX(), 'f', _precision)).arg(QString::number(n->getY(), 'f', _precision)));
  }

  _writer->writeCharacters(QString(")"));
  _writer->writeEndElement();

  _writeTags(w);
}

void OsmGbdxXmlWriter::writePartial(const ConstWayPtr& w)
{
  LOG_VARI(w);

  if (_includePointInWays)
  {
    throw HootException("Adding points to way output is not supported in streaming output.");
  }

  _writer->writeStartElement("way");
  _writer->writeAttribute("visible", "true");
  _writer->writeAttribute("id", QString::number(w->getId()));

  for (size_t j = 0; j < w->getNodeCount(); j++)
  {
    _writer->writeStartElement("nd");
    _writer->writeAttribute("ref", QString::number(w->getNodeId(j)));
    _writer->writeEndElement();
  }

  _writeTags(w);

  _writer->writeEndElement();
}

void OsmGbdxXmlWriter::writePartial(const ConstRelationPtr& r)
{
  LOG_VART(r);

  _writer->writeStartElement("relation");
  _writer->writeAttribute("visible", "true");
  _writer->writeAttribute("id", QString::number(r->getId()));

  const vector<RelationData::Entry>& members = r->getMembers();
  for (size_t j = 0; j < members.size(); j++)
  {
    const RelationData::Entry& e = members[j];
    _writer->writeStartElement("member");
    _writer->writeAttribute("type", _typeName(e.getElementId().getType()));
    _writer->writeAttribute("ref", QString::number(e.getElementId().getId()));
    _writer->writeAttribute("role", removeInvalidCharacters(e.role));
    _writer->writeEndElement();
  }

  _writeTags(r);

  _writer->writeEndElement();
}

void OsmGbdxXmlWriter::finalizePartial()
{
  //osmosis chokes on the bounds being written at the end of the file, so not writing it at all
  //_writeBounds(_bounds);
  close();
}

}
