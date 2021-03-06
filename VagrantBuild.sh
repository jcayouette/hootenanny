#!/usr/bin/env bash

set -e

# Set this for use later
export OS_NAME="$(lsb_release -i -s)"

if [ "$OS_NAME" == "Ubuntu" ]; then
  source ~/.profile
else # Centos
  source ~/.bash_profile
fi

if [ -z "$HOOT_HOME" ]; then
    HOOT_HOME=~/hoot
fi
echo HOOT_HOME: $HOOT_HOME

cd $HOOT_HOME
source ./SetupEnv.sh

echo "### Configuring Hoot..."
echo HOOT_HOME: $HOOT_HOME

# Going to remove this so that it gets updated
if [ -f missing ]; then
  rm -f missing
fi

# Taking out ui-tests until we get Tomcat8 etc installed
aclocal && autoconf && autoheader && automake --add-missing --copy && ./configure --quiet --with-rnd --with-services --with-uitests

if [ ! -f LocalConfig.pri ] && ! grep --quiet QMAKE_CXX LocalConfig.pri; then
    echo 'Customizing LocalConfig.pri...'
    cp LocalConfig.pri.orig LocalConfig.pri
    sed -i s/"QMAKE_CXX=g++"/"#QMAKE_CXX=g++"/g LocalConfig.pri
    sed -i s/"#QMAKE_CXX=ccache g++"/"QMAKE_CXX=ccache g++"/g LocalConfig.pri
fi

echo "Building Hoot... "
make -s clean && make -sj$(nproc)

# vagrant will auto start the tomcat service for us, so just copy the web app files w/o manipulating the server
# Copy the web apps, no need to use sudo
./scripts/tomcat/CopyWebAppsToTomcat.sh

# docs build is always failing the first time during the npm install portion for an unknown reason, but then
# always passes the second time its run...needs fixed, but this is the workaround for now
echo "Building Hoot docs... "
make -sj$(nproc) docs &> /dev/null || true
make -sj$(nproc) docs

hoot version

echo "See VAGRANT.md for additional configuration instructions and then run 'vagrant ssh' to log into the Hootenanny virtual machine."
echo "See $HOOT_HOME/docs on the virtual machine for Hootenanny documentation files."
echo "Access the web application at http://localhost:8888/hootenanny-id"
echo "If you wish to run the diagnostic tests, log into the virtual machine and run: 'cd $HOOT_HOME && make -sj$(nproc) test-all'"
