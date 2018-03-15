#!/usr/bin/env bash

set -e

# Embed version into ChromeDriver ZIP filename.
SELENIUM_FULL_VERSION=$SELENIUM_VERSION.$SELENIUM_PATCH
SELENIUM_FILE=selenium-server-standalone-$SELENIUM_FULL_VERSION.jar
SELENIUM_URL=https://selenium-release.storage.googleapis.com/$SELENIUM_VERSION/$SELENIUM_FILE

if [ ! -f $HOME/bin/$SELENIUM_FILE ] ; then
    echo "### Installing Selenium standalone server v${SELENIUM_FULL_VERSION}..."
    mkdir -p $HOME/bin
    curl -sSL -o $HOME/bin/$SELENIUM_FILE $SELENIUM_URL
fi

