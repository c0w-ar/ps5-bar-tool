# ps5-bar-tool

PS5 implements a feature called "Backup and Restore" (BAR) to allow users to move or save users and apps information.

This payload let's you visualize the information of any BAR file and dump the embedded file system using the PS5 as oracle.

This is WIP.

## Credits

[john-tornblom](https://github.com/john-tornblom): PS5 SDK (https://github.com/ps5-payload-dev/sdk)

## Own compilation

First install the ps5-payload-sdk

```sh
make
```

## How to Use

Place the PS5 BAR file in /mnt/usb0/PS5/EXPORT/BACKUP/archive.dat

```
USB0 Root
└── 📁 PS5
    └── 📁 EXPORT
        └── 📁 BACKUP
            └── 📄 archive.dat
            └── 📄 archive0001.dat (if multi file)
            └── 📄 archive0002.dat (if multi file)
            └── 📄 archiveXXXX.dat
            └── 📁 DUMP (output)
            └── 📄 ps5-bar-tool.log (output)
```

Send the payload

```sh
socat -t 99999 - TCP:YOUR_PS5_IP:9021 < ps5-bar-tool.elf
```

Wait until it finishes.

You'll find a log file in the folder.