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
 * @copyright Copyright (C) 2017 DigitalGlobe (http://www.digitalglobe.com/)
 * @copyright Copyright (C) 2017, 2018 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "ImplicitTagRawRulesDeriver.h"

// hoot
#include <hoot/core/io/OsmMapReaderFactory.h>
#include <hoot/core/io/PartialOsmMapReader.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/elements/Tags.h>
#include <hoot/core/util/StringUtils.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/io/ElementVisitorInputStream.h>
#include <hoot/core/visitors/TranslationVisitor.h>
#include <hoot/core/util/FileUtils.h>
#include <hoot/core/algorithms/Translator.h>
#include <hoot/core/filters/ImplicitTagEligiblePoiCriterion.h>
#include <hoot/core/filters/ImplicitTagEligiblePoiPolyCriterion.h>
#include <hoot/core/schema/ImplicitTagUtils.h>

// Qt
#include <QStringBuilder>
#include <QThread>

namespace hoot
{

ImplicitTagRawRulesDeriver::ImplicitTagRawRulesDeriver() :
_statusUpdateInterval(ConfigOptions().getTaskStatusUpdateInterval()),
_countFileLineCtr(0),
_sortParallelCount(QThread::idealThreadCount()),
_skipFiltering(false),
_keepTempFiles(false),
_tempFileDir(ConfigOptions().getApidbBulkInserterTempFileDir()),
_translateAllNamesToEnglish(true)
{
}

void ImplicitTagRawRulesDeriver::setConfiguration(const Settings& conf)
{
  ConfigOptions options = ConfigOptions(conf);
  setSortParallelCount(options.getImplicitTaggingRawRulesDeriverSortParallelCount());
  const int idealThreads = QThread::idealThreadCount();
  LOG_VART(idealThreads);
  if (_sortParallelCount < 1 || _sortParallelCount > idealThreads)
  {
    setSortParallelCount(idealThreads);
  }
  setSkipFiltering(options.getImplicitTaggingRawRulesDeriverSkipFiltering());
  setKeepTempFiles(options.getImplicitTaggingKeepTempFiles());
  setTempFileDir(options.getApidbBulkInserterTempFileDir());
  setTranslateAllNamesToEnglish(options.getImplicitTaggingTranslateAllNamesToEnglish());
}

void ImplicitTagRawRulesDeriver::setElementFilter(const QString type)
{
  if (type.trimmed().toUpper() != "POI")
  {
    _elementFilter.reset(new ImplicitTagEligiblePoiCriterion());
  }
  else if (type.trimmed().toUpper() != "POI-POLYGON")
  {
    _elementFilter.reset(new ImplicitTagEligiblePoiPolyCriterion());
  }
  else
  {
    throw HootException("Invalid element filter type: " + type);
  }
}

void ImplicitTagRawRulesDeriver::_init()
{
  _wordKeysToCountsValues.clear();
  _duplicatedWordTagKeyCountsToValues.clear();
  _countFileLineCtr = 0;

  _countFile.reset(
    new QTemporaryFile(_tempFileDir + "/poi-implicit-tag-raw-rules-generator-temp-XXXXXX"));
  _countFile->setAutoRemove(!_keepTempFiles);
  if (!_countFile->open())
  {
    throw HootException(QObject::tr("Error opening %1 for writing.").arg(_countFile->fileName()));
  }
  LOG_DEBUG("Opened temp file: " << _countFile->fileName());
  if (_keepTempFiles)
  {
    LOG_WARN("Keeping temp file: " << _countFile->fileName());
  }
}

void ImplicitTagRawRulesDeriver::deriveRawRules(const QStringList inputs,
                                                const QStringList translationScripts,
                                                const QString output)
{
  _validateInputs(inputs, translationScripts, output);

  LOG_INFO(
    "Generating POI implicit tag rules raw file for inputs: " << inputs <<
    ", translation scripts: " << translationScripts << ".  Writing to output: " << output << "...");
  LOG_VARD(_sortParallelCount);
  LOG_VARD(_skipFiltering);;
  LOG_VARD(_translateAllNamesToEnglish);

  _init();

  long poiCount = 0;
  long nodeCount = 0;
  for (int i = 0; i < inputs.size(); i++)
  {
    boost::shared_ptr<ElementInputStream> inputStream =
      _getInputStream(inputs.at(i), translationScripts.at(i));
    while (inputStream->hasMoreElements())
    {
      ElementPtr element = inputStream->readNextElement();
      LOG_VART(element);

      nodeCount++;

      assert(_elementFilter.get());
      if (_skipFiltering || _elementFilter->isSatisfied(element))
      {
        QStringList names = element->getTags().getNames();
        assert(!names.isEmpty());

        //old_name generally indicates that an element formerly went by the name, so not really
        //useful here.
        if (names.removeAll("old_name") > 0)
        {
          LOG_VARD("Removed old name tag.");
        }
        assert(!names.isEmpty());

        if (_translateAllNamesToEnglish)
        {
          names = ImplicitTagUtils::translateNamesToEnglish(names, element->getTags());
        }
        LOG_VART(names);

        //get back only the tags that we'd be interested in applying to future elements implicitly
        //based on name
        const QStringList kvps = _elementFilter->getEligibleKvps(element->getTags());
        assert(!kvps.isEmpty());
        if (kvps.isEmpty())
        {
          throw HootException("POI kvps empty.");
        }

        //parse whole names and token groups
        _parseNames(names, kvps);

        poiCount++;

        if (poiCount % _statusUpdateInterval == 0)
        {
          PROGRESS_INFO(
            "Parsed " << StringUtils::formatLargeNumber(poiCount) << " eligible POIs / " <<
            StringUtils::formatLargeNumber(nodeCount) << " nodes.");
        }
      }
    }
    _inputReader->finalizePartial();
  }
  _countFile->close();

  LOG_INFO(
    "Parsed " << StringUtils::formatLargeNumber(poiCount) << " POIs from " <<
    StringUtils::formatLargeNumber(nodeCount) << " nodes.");
  LOG_INFO(
    "Wrote " << StringUtils::formatLargeNumber(_countFileLineCtr) << " lines to count file.");

  _sortByTagOccurrence();   //sort in descending count order
  _removeDuplicatedKeyTypes();
  bool tieCountsNeededResolved = false;
  if (_duplicatedWordTagKeyCountsToValues.size() > 0)
  {
    _resolveCountTies();
    tieCountsNeededResolved = true;
  }
  LOG_INFO(
    "Extracted "  << StringUtils::formatLargeNumber(_wordKeysToCountsValues.size()) <<
    " word/tag associations.");
  _wordKeysToCountsValues.clear();
  if (tieCountsNeededResolved)
  {
    _sortByWord(_tieResolvedCountFile);
  }
  else
  {
    _sortByWord(_dedupedCountFile);
  }
}

void ImplicitTagRawRulesDeriver::_validateInputs(const QStringList inputs,
                                                 const QStringList translationScripts,
                                                 const QString output)
{
  LOG_VARD(inputs);
  LOG_VARD(translationScripts);
  LOG_VARD(output);

  if (!_elementFilter.get())
  {
    throw HootException("No element type was specified.");
  }
  if (inputs.isEmpty())
  {
    throw HootException("No inputs were specified.");
  }
  if (inputs.size() != translationScripts.size())
  {
    LOG_VARD(inputs.size());
    LOG_VARD(translationScripts.size());
    throw HootException(
      "The size of the input datasets list must equal the size of the list of translation scripts.");
  }
  if (output.isEmpty())
  {
    throw HootException("No output was specified.");
  }
  _output.reset(new QFile());
  _output->setFileName(output);
  if (_output->exists() && !_output->remove())
  {
    throw HootException(QObject::tr("Error removing existing %1 for writing.").arg(output));
  }
  _output->close();
}

void ImplicitTagRawRulesDeriver::_updateForNewWord(QString word, const QString kvp)
{
  word = word.simplified();
  LOG_TRACE("Updating word: " << word << " with kvp: " << kvp << "...");

  ImplicitTagUtils::cleanName(word);

  if (!word.isEmpty())
  {
    bool wordIsNumber = false;
    word.toLong(&wordIsNumber);
    if (wordIsNumber)
    {
      LOG_TRACE("Skipping word: " << word << ", which is a number.");
      return;
    }

    const bool hasAlphaChar = StringUtils::hasAlphabeticCharacter(word);
    if (!hasAlphaChar)
    {
      LOG_TRACE("Skipping word: " << word << ", which has no alphabetic characters.");
      return;
    }

    const QString line = word.toLower() % QString("\t") % kvp % QString("\n");
    _countFile->write(line.toUtf8());
    _countFileLineCtr++;
  }
}

boost::shared_ptr<ElementInputStream> ImplicitTagRawRulesDeriver::_getInputStream(
  const QString input, const QString translationScript)
{
  LOG_INFO("Parsing: " << input << "...");

  _inputReader =
    boost::dynamic_pointer_cast<PartialOsmMapReader>(
      OsmMapReaderFactory::getInstance().createReader(input));
  _inputReader->open(input);
  boost::shared_ptr<ElementInputStream> inputStream =
    boost::dynamic_pointer_cast<ElementInputStream>(_inputReader);
  LOG_VARD(translationScript);
  //"none" allows for bypassing translation for an input; e.g. OSM data
  if (translationScript.toLower() != "none")
  {
    boost::shared_ptr<TranslationVisitor> translationVisitor(new TranslationVisitor());
    translationVisitor->setPath(translationScript);
    inputStream.reset(new ElementVisitorInputStream(_inputReader, translationVisitor));
  }
  return inputStream;
}

void ImplicitTagRawRulesDeriver::_parseNames(const QStringList names, const QStringList kvps)
{
  for (int i = 0; i < names.size(); i++)
  {
    QString name = names.at(i);
    LOG_VART(name);

    //'=' is used in the map key for kvps, so it needs to be escaped in the word
    if (name.contains("="))
    {
      name = name.replace("=", "%3D");
    }
    for (int j = 0; j < kvps.size(); j++)
    {
      _updateForNewWord(name, kvps.at(j));
    }

    QStringList nameTokens = _tokenizer.tokenize(name);
    LOG_VART(nameTokens.size());

    //tokenization

    for (int j = 0; j < nameTokens.size(); j++)
    {
      QString nameToken = nameTokens.at(j);
      _parseNameToken(nameToken, kvps);
    }

    //going up to a token group size of two; tested up to group size three, but three didn't seem to
    //yield any better tagging results
    if (nameTokens.size() > 2)
    {
      for (int j = 0; j < nameTokens.size() - 1; j++)
      {
        QString nameToken = nameTokens.at(j) + " " + nameTokens.at(j + 1);
        _parseNameToken(nameToken, kvps);
      }
    }
  }
}

void ImplicitTagRawRulesDeriver::_parseNameToken(QString& nameToken, const QStringList kvps)
{
  //may eventually need to replace more punctuation chars here, but this is fine for now...need a
  //more extensible way to do it; also, that logic could moved into ImplicitTagUtils::cleanName
  nameToken = nameToken.replace(",", "");
  LOG_VART(nameToken);

  if (_translateAllNamesToEnglish)
  {
    const QString englishNameToken = Translator::getInstance().toEnglish(nameToken);
    nameToken = englishNameToken;
    LOG_VART(englishNameToken);
  }

  for (int k = 0; k < kvps.size(); k++)
  {
    _updateForNewWord(nameToken, kvps.at(k));
  }
}

void ImplicitTagRawRulesDeriver::_sortByTagOccurrence()
{
  LOG_INFO("Sorting output by tag occurrence count...");
  LOG_VART(_sortParallelCount);

  _sortedCountFile.reset(
    new QTemporaryFile(_tempFileDir + "/poi-implicit-tag-raw-rules-generator-temp-XXXXXX"));
  _sortedCountFile->setAutoRemove(!_keepTempFiles);
  if (!_sortedCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for writing.").arg(_sortedCountFile->fileName()));
  }
  LOG_DEBUG("Opened sorted temp file: " << _sortedCountFile->fileName());
  if (_keepTempFiles)
  {
    LOG_WARN("Keeping temp file: " << _sortedCountFile->fileName());
  }

  //This counts each unique line occurrence, sorts by decreasing occurrence count (necessary for
  //next step which removes duplicate tag keys associated with the same word), and replaces the
  //space between the prepended count and the word with a tab.

  //sort by highest count, then by word, then by tag
  const QString cmd =
    "sort --parallel=" + QString::number(_sortParallelCount) + " " + _countFile->fileName() +
    " | uniq -c | sort -n -r --parallel=" + QString::number(_sortParallelCount) + " | " +
    "sed -e 's/^ *//;s/ /\t/' > " + _sortedCountFile->fileName();
  if (std::system(cmd.toStdString().c_str()) != 0)
  {
    throw HootException("Unable to sort file.");
  }
  LOG_INFO(
    "Wrote " <<
    StringUtils::formatLargeNumber(FileUtils::getNumberOfLinesInFile(_sortedCountFile->fileName())) <<
    " lines to sorted file.");
}

void ImplicitTagRawRulesDeriver::_removeDuplicatedKeyTypes()
{
  LOG_INFO("Removing duplicated tag key types from output...");

  //i.e. don't allow amenity=school AND amenity=shop to be associated with the same word...pick one
  //of them

  _dedupedCountFile.reset(
    new QTemporaryFile(
      _tempFileDir + "/poi-implicit-tag-raw-rules-generator-temp-XXXXXX"));
  _dedupedCountFile->setAutoRemove(!_keepTempFiles);
  if (!_dedupedCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for writing.").arg(_dedupedCountFile->fileName()));
  }
  LOG_DEBUG("Opened dedupe temp file: " << _dedupedCountFile->fileName());
  if (_keepTempFiles)
  {
    LOG_WARN("Keeping temp file: " << _dedupedCountFile->fileName());
  }

  long lineCount = 0;
  long writtenLineCount = 0;
  while (!_sortedCountFile->atEnd())
  {
    const QString line = QString::fromUtf8(_sortedCountFile->readLine().constData()).trimmed();
    LOG_VART(line);
    const QStringList lineParts = line.split("\t");
    LOG_VART(lineParts);
    QString word = lineParts[1].trimmed();
    LOG_VART(word);
    const QString kvp = lineParts[2].trimmed();
    LOG_VART(kvp);
    const QString countStr = lineParts[0].trimmed();
    const long count = countStr.toLong();
    LOG_VART(count);
    const QStringList kvpParts = kvp.split("=");
    const QString tagKey = kvpParts[0];
    LOG_VART(tagKey);
    const QString wordTagKey = word.trimmed() % ";" % tagKey.trimmed();
    LOG_VART(wordTagKey);
    const QString wordTagKeyCount =
      word.trimmed() % ";" % tagKey.trimmed() % ";" % countStr.trimmed();
    LOG_VART(wordTagKeyCount);
    const QString tagValue = kvpParts[1];
    LOG_VART(tagValue);

    //The lines are sorted in reverse by occurrence count.  So the first time we see one word/key
    //combo, we know it had the highest occurrence count, and we can ignore all subsequent
    //instances of it since any one feature can't have more than one tag applied to it with the
    //same key.

    const QString queriedCountAndValue = _wordKeysToCountsValues.value(wordTagKey, "");
    if (queriedCountAndValue.isEmpty())
    {
      _wordKeysToCountsValues[wordTagKey] = countStr % ";" % tagValue;
      //this unescaping must occur during the final temp file write
      if (word.contains("%3D"))
      {
        word = word.replace("%3D", "=");
      }
      else if (word.contains("%3d"))
      {
        word = word.replace("%3d", "=");
      }
      const QString updatedLine = countStr % "\t" % word % "\t" % kvp % "\n";
      LOG_VART(updatedLine);
      _dedupedCountFile->write(updatedLine.toUtf8());
      writtenLineCount++;
    }
    else
    {
      const long queriedCount = queriedCountAndValue.split(";")[0].toLong();
      if (queriedCount == count)
      {
        LOG_TRACE(
          "Recording duplicated word/tag key/count for: " << wordTagKeyCount << " with value: " <<
          tagValue);
        _duplicatedWordTagKeyCountsToValues[wordTagKeyCount].append(tagValue);
      }
    }

    lineCount++;
    if (lineCount % (_statusUpdateInterval * 10) == 0)
    {
      PROGRESS_INFO(
        "Parsed " << StringUtils::formatLargeNumber(lineCount) <<
        " lines from input for duplicated tag key removal.");
    }
  }
  _sortedCountFile->close();
  LOG_INFO(
    "Wrote " << StringUtils::formatLargeNumber(writtenLineCount) << " lines to deduped file.");
  _dedupedCountFile->close();
}

void ImplicitTagRawRulesDeriver::_resolveCountTies()
{
  //Any time more than one word/key combo has the same occurrence count, we need to pick just one
  //of them.

  LOG_INFO(
    "Resolving word/tag key/count ties for " <<
    StringUtils::formatLargeNumber(_duplicatedWordTagKeyCountsToValues.size()) <<
    " duplicated word/tag key/counts...");

  _tieResolvedCountFile.reset(
    new QTemporaryFile(
      _tempFileDir + "/poi-implicit-tag-raw-rules-generator-temp-XXXXXX"));
  _tieResolvedCountFile->setAutoRemove(!_keepTempFiles);
  if (!_tieResolvedCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for writing.").arg(_tieResolvedCountFile->fileName()));
  }
  LOG_DEBUG("Opened tie resolve temp file: " << _tieResolvedCountFile->fileName());
  if (_keepTempFiles)
  {
    LOG_WARN("Keeping temp file: " << _tieResolvedCountFile->fileName());
  }
  if (!_dedupedCountFile->open())
  {
    throw HootException(
      QObject::tr("Error opening %1 for reading.").arg(_dedupedCountFile->fileName()));
  }

  long lineCount = 0;
  long duplicateResolutions = 0;
  while (!_dedupedCountFile->atEnd())
  {
    const QString line = QString::fromUtf8(_dedupedCountFile->readLine().constData()).trimmed();
    LOG_VART(line);
    const QStringList lineParts = line.split("\t");
    LOG_VART(lineParts);
    QString word = lineParts[1].trimmed();
    LOG_VART(word);
    const QString kvp = lineParts[2].trimmed();
    LOG_VART(kvp);
    const QString countStr = lineParts[0].trimmed();
    const long count = countStr.toLong();
    LOG_VART(count);
    const QStringList kvpParts = kvp.split("=");
    const QString tagKey = kvpParts[0];
    LOG_VART(tagKey);
    const QString wordTagKey = word.trimmed() % ";" % tagKey.trimmed();
    LOG_VART(wordTagKey);
    const QString wordTagKeyCount =
      word.trimmed() % ";" % tagKey.trimmed() % ";" % countStr.trimmed();
    LOG_VART(wordTagKeyCount);
    const QString tagValue = kvpParts[1];
    LOG_VART(tagValue);

    if (_duplicatedWordTagKeyCountsToValues.contains(wordTagKeyCount))
    {
      LOG_TRACE("Resolving duplicated word/tag key/count for " << wordTagKeyCount << "...");

      //To resolve the tie, we're going to pick the most specific kvp.  e.g. amenity=public_hall
      //wins out of amenity=hall.  This is not really dealing with same hierarchy level tags
      //(e.g. amenity=school and amenity=hall) and will just arbitrarily pick in that situation.
      //Duplicates do seem to be fairly rare, but there could be some perfomance gains by coming
      //up with a better way to handle this situation.
      QString lineWithMostSpecificKvp = line % "\n";
      const QStringList tagValues = _duplicatedWordTagKeyCountsToValues[wordTagKeyCount];
      for (int i = 0; i < tagValues.size(); i++)
      {
        const QString childKvp = tagKey % "=" % tagValues[i];
        if (OsmSchema::getInstance().isAncestor(childKvp, tagKey % "=" % tagValue))
        {
          lineWithMostSpecificKvp = countStr % "\t" % word % "\t" % childKvp % "\n";
        }
      }
      LOG_VART(lineWithMostSpecificKvp);
      _tieResolvedCountFile->write(lineWithMostSpecificKvp.toUtf8());
      duplicateResolutions++;
    }
    else
    {
      const QString updatedLine = countStr % "\t" % word % "\t" % kvp % "\n";
      LOG_VART(updatedLine);
      _tieResolvedCountFile->write(updatedLine.toUtf8());
    }

    lineCount++;
    if (lineCount % (_statusUpdateInterval * 10) == 0)
    {
      PROGRESS_INFO(
        "Parsed " << StringUtils::formatLargeNumber(lineCount) <<
        " lines from input for duplicated tag key count ties.");
    }
  }
  LOG_VARD(lineCount);
  LOG_INFO(
    "Resolved " << StringUtils::formatLargeNumber(duplicateResolutions) <<
    " word/tag key/count ties.");
  _duplicatedWordTagKeyCountsToValues.clear();
  _tieResolvedCountFile->close();
}

void ImplicitTagRawRulesDeriver::_sortByWord(boost::shared_ptr<QTemporaryFile> input)
{
  LOG_INFO("Sorting output by word...");

  //sort by word, then by tag
  const QString cmd =
    "sort -t'\t' -k2,2 -k3,3 --parallel=" + QString::number(_sortParallelCount) + " " +
     input->fileName() + " -o " + _output->fileName();
  if (std::system(cmd.toStdString().c_str()) != 0)
  {
    throw HootException("Unable to sort input file.");
  }

  LOG_VARD(_output->fileName());
  LOG_INFO(
    "Wrote " <<
    StringUtils::formatLargeNumber(
      FileUtils::getNumberOfLinesInFile(_output->fileName())) << " lines to final sorted file.");
}

}
