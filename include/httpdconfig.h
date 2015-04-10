
//Define this if you want to be able to use Heatshrink-compressed espfs images.
#define EFS_HEATSHRINK

//Pos of esp fs in flash
#define ESPFS_POS 0x12000 // to 0x26FFF if mmem size is 0x19000 so size is 86015

//If you want, you can define a realm for the authentication system.
//#define HTTP_AUTH_REALM "MyRealm"