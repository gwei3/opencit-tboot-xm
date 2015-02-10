#!/bin/bash

OUTFILE="/rootfs.tar.gz"
KERNEL_VERSION=`uname -r`
WORKING_DIR=`pwd`


echo -e "\nThis script will create tarball of the whole filesystem (excluding the current working directory)"
echo -e "All logs in /var/logs will be cleared by the script.\n"
echo -e "Do you want to continue? (y/n)"
read response

if [ "$response" != "y" ]; then
    echo "Cancelled on user request"
    exit 1
fi

echo "Creating rootfs for KVM/XEN?"
read input

if [ $input != "XEN" ] && [ $input != "KVM" ] ; then
    echo "Please Enter valid choice KVM/XEN"
    exit 1
fi

if [ $input == "XEN" ]; then
    	PREGENERATED_FILES=xen_pre_generated_files
	#Clean any pre-existing rootfs file in the folder
	rm $WORKING_DIR/$PREGENERATED_FILES/rootfs.tar.gz

else
    	PREGENERATED_FILES=kvm_pre_generated_files
	#Clean any pre-existing rootfs file in the folder
	rm $WORKING_DIR/$PREGENERATED_FILES/rootfs.tar.gz
fi

#cleanup logs first
for file in `find /var/log/ -type f`; 
do
    if file --mime-type "$file" | grep -q gzip$; then
        rm -f $file
    else
        cat /dev/null > $file
    fi
done

#clean cached files by apt
apt-get clean
apt-get autoclean

#modify motd
unlink /etc/motd
echo "" > /etc/motd
echo " * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *" >> /etc/motd
echo " * TCB protection project                                                    *" >> /etc/motd
echo " * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *" >> /etc/motd

exclude_lib_versions=`ls /lib/modules/ | grep -v $KERNEL_VERSION`
exclude_str=""
for version in $exclude_lib_versions
do
    exclude_str="$exclude_str --exclude=/lib/modules/$version/* "
done

exclude_boot_dir_files=`ls /boot/ | grep -v $KERNEL_VERSION`
for exclude_file in $exclude_boot_dir_files
do
    exclude_str="$exclude_str --exclude=/boot/$exclude_file "
done

SOURCE_DIR=$(echo $0 | sed -e s/create_rootfs_tarball.sh//)

if [ $input == "KVM" ];then
    mv /etc/rc.local /etc/rc.local.bak
    mv /etc/fstab /etc/fstab.bak
    trap 'mv /etc/rc.local.bak /etc/rc.local 2> /dev/null ; mv /etc/fstab.bak /etc/fstab 2> /dev/null' SIGHUP SIGINT SIGTERM    

    BASE_DIR=$(grep cd /etc/rc.local.bak | sed -e s?.*/opt?/opt? -e s?bin/debug??)
    IPADDR=$(grep RPCORE_IPADDR /etc/rc.local.bak)
    echo "ifdown --exclude=lo -a &&  ifup --exclude=lo -a
/bin/storage_mount
${IPADDR}
export RPCORE_PORT=16005
export LD_LIBRARY_PATH=${BASE_DIR}lib:
cp -r ${BASE_DIR}rptmp/* /tmp/rptmp
cd ${BASE_DIR}bin/debug
nohup ./nontpmrpcore &
killall -9 libvirtd
killall -9 rp_listner
sleep 2
ldconfig
nohup ./rp_listner &
libvirtd -d
sleep 1
/root/services.sh restart
chown -R nova:nova /var/run/libvirt/ /var/log/nova/
exit 0" > /etc/rc.local
    chmod +x /etc/rc.local

    touch /etc/fstab
    echo "# Generated by TCB protection scripts" > /etc/fstab 
    cp ${SOURCE_DIR}scripts/storage_mount /bin/
    chmod +x /bin/storage_mount
    
    cd /
    
    tar $exclude_str                           	 \
    --exclude="/etc/udev/rules.d/*-persistent-*" \
    --exclude="$WORKING_DIR/*"                   \
    --exclude="/lost+found"                      \
    --exclude="/media/*"                         \
    --exclude="/mnt/*"                           \
    --exclude="/proc/*"                          \
    --exclude="/sys/*"                           \
    --exclude="/tmp/*"                           \
    --exclude="/var/lib/nova/images/*"           \
    --exclude="/var/lib/nova/instances/*"        \
    --exclude="/var/lib/libvirt/images/*"        \
    --exclude="/var/tmp/*"                       \
    --exclude="/var/spool/*"                     \
    --exclude="/usr/share/doc/*"                 \
    --exclude="/usr/share/man/*"                 \
    --exclude="/usr/src/*"                       \
    --exclude="/var/cache/*"                     \
    --exclude="/*gz"                             \
    --exclude="/etc/rc.local.bak"                \
    --exclude="/etc/fstab.bak"                   \
    --exclude="/*iso"				 \
    --exclude="/root/jdk*"			 \
    --exclude="/root/test*"			 \
    -zcvpf rootfs.tar.gz /

    mv /etc/fstab.bak /etc/fstab
    mv /etc/rc.local.bak /etc/rc.local
    mv /rootfs.tar.gz $WORKING_DIR/$PREGENERATED_FILES
fi



if [ $input == "XEN" ];then
    mkdir /storage
    cp -r /var/lib/xcp /storage/
    rm -r /var/lib/xcp
    ln -s /storage/xcp /var/lib/xcp

    mv /etc/fstab /etc/fstab.bak
    touch /etc/fstab
    echo "# Generated by TCB protection scripts" >/etc/fstab
    echo "/dev/mapper/crypt-storage /storage ext4 defaults 0 1" > /etc/fstab
    
    mv /etc/rc.local /etc/rc.local.bak
    touch /etc/rc.local
    echo "/bin/storage_mount" > /etc/rc.local
    echo "/etc/init.d/networking restart" >> /etc/rc.local
    echo "sr_dir=\`df -h | grep /run/sr-mount/ | awk -F' ' '{print \$6}'\`; if [ -d \$sr_dir/guest ]; then ln -s \$sr_dir/guest /boot/; fi" >> /etc/rc.local
    echo "service xcp-xapi restart" >> /etc/rc.local
    echo "exit 0" >> /etc/rc.local
    chmod +x /etc/rc.local

    cp ${SOURCE_DIR}scripts/storage_mount_xen /bin/storage_mount
    chmod +x /bin/storage_mount
   
    trap 'mv /etc/rc.local.bak /etc/rc.local 2> /dev/null; mv /etc/fstab.bak /etc/fstab 2> /dev/null; unlink /var/lib/xcp 2> /dev/null; cp -r /storage/xcp  /var/lib/xcp 2> /dev/null; rm -r /storage 2> /dev/null' SIGHUP SIGINT SIGTERM

    cd /

    tar $exclude_str                             \
    --exclude="/etc/udev/rules.d/*-persistent-*" \
    --exclude="$WORKING_DIR/*"                   \
    --exclude="/lost+found"                      \
    --exclude="/media/*"                         \
    --exclude="/mnt/*"                           \
    --exclude="/proc/*"                          \
    --exclude="/sys/*"                           \
    --exclude="/tmp/*"                           \
    --exclude="/var/tmp/*"                       \
    --exclude="/var/run/sr-mount/*"              \
    --exclude="/run/sr-mount/*"                  \
    --exclude="/var/spool/*"                     \
    --exclude="/var/cache/*"                     \
    -zcvpf rootfs.tar.gz /

    mv /etc/rc.local.bak /etc/rc.local
    mv /etc/fstab.bak /etc/fstab
    unlink /var/lib/xcp
    cp -r /storage/xcp  /var/lib/xcp
    rm -r /storage
    mv /rootfs.tar.gz $WORKING_DIR/$PREGENERATED_FILES

fi
#restore motd
rm -f /etc/motd
ln -s /var/run/motd /etc/motd

echo "Created tar at location $WORKING_DIR/kvm_pre_generated_files/rootfs.tar.gz"

