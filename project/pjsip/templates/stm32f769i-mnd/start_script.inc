//"diag -o",
"mkdir -v /conf",
"mount -t DumbFS /dev/qspiflash0 /conf",
"conf_setup",
"netmanager",
"arping -c 1 -l",
"diag -m",
"simple_pjsua_imported"
