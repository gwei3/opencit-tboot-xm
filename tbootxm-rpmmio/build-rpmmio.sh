#!/bin/bash

CURR_ROOT=$PWD

BIN_DIR=$CURR_ROOT/bin
if [ -n $1 ]; then
    BIN_DIR=$1
fi

function getFlavour()
{
        flavour=""
        grep -c -i ubuntu /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="ubuntu"
        fi
        grep -c -i "red hat" /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="rhel"
        fi
        grep -c -i fedora /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="fedora"
        fi
        grep -c -i suse /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="suse"
        fi
        if [ "$flavour" == "" ] ; then
                echo "Unsupported linux flavor, Supported versions are ubuntu, rhel, fedora"
                exit
        else
                echo $flavour
        fi
}

FLAVOUR=`getFlavour`

if [ $FLAVOUR == "fedora" -o $FLAVOUR == "rhel" ]
then
	MODULE_DIR="/lib/modules"
#	INSTALLED_DEVEL_HEADERS=`ls /usr/src/kernels/ | grep -v debug`
elif [ $FLAVOUR == "ubuntu" ]
then
	MODULE_DIR="/lib/modules"
#	INSTALLED_DEVEL_HEADERS=`ls /usr/src/ | grep generic | sed 's/linux-headers-//g'`	
fi

INSTALLED_KERNEL_HEADERS=`ls $MODULE_DIR`

BUILD_LOG=_buildlog

cd src/
rpmmio_directories=$(find . -maxdepth 1 -mindepth 1 -type d | cut -c 3-)
for directory in ${rpmmio_directories[@]}; do
    echo $directory
    RPMMIO_DIR=$BIN_DIR/$directory
    echo $RPMMIO_DIR
    mkdir -p $RPMMIO_DIR
    LOG_FILE=$CURR_ROOT/$directory$BUILD_LOG
    echo $LOG_FILE
    cd $directory
    for HEADER in $INSTALLED_KERNEL_HEADERS ; do
    	if [ -e $MODULE_DIR/$HEADER/build ]; then
	    make clean >> $LOG_FILE 2>&1
	    if [ `echo $?` -ne 0 ]
	    then
		echo "ERROR: Could not clean rpmmio"
		exit 1
	    fi
	    echo "Building rpmmio.ko for $HEADER"
	    make KDIR=$MODULE_DIR/$HEADER/build >> $LOG_FILE 2>&1
	    if [ `echo $?` -ne 0 ]
	    then
		echo "ERROR: Could not make rpmmio"
		exit 1
	    fi
	    mv rpmmio.ko $RPMMIO_DIR/rpmmio-$HEADER.ko
	else
	    echo "WARNING: Header files doesn't available for kernel $HEADER"
	fi
    done
    cd ..
done
cd ..
