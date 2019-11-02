/*
 * 
 * network.h
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 *  Network.h reads network configuration from file system and sets both ESP32 network interfaces accordingly
 *
 *  It is a little awkward why UNIX, LINUX, Raspbian are using so many network configuration files and how they are used):
 * 
 *    /network/interfaces             - modify the code below with your IP addresses
 *    /etc/wpa_supplicant.conf        - modify the code below with your WiFi SSID and password (this file is used instead of /etc/wpa_supplicant/wpa_supplicant.conf, the latter name is too long to fit into SPIFFS)
 *    /etc/dhcpcd.conf                - modify the code below with your access point IP addresses 
 *    /etc/hostapd/hostapd.conf       - modify the code below with your access point SSID and password
 * 
 * History:
 *          - first release, 
 *            November 16, 2018, Bojan Jurca
 *          - added ifconfig, arp -a, 
 *            December 9, 2018, Bojan Jurca
 *          - added iw, 
 *            December 11, 2018, Bojan Jurca
  *         - added SPIFFSsemaphore to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            simplified installation,
 *            April 13, 2019, Bojan Jurca
 *          - arp command improvement - now a pointer to arp table is obtained during initialization - more likely to be successful
 *            April 21, 2019, Bojan Jurca
 *          - added network event logging, 
 *            the use of dmesg 
 *            September 14, 2019, Bojan Jurca
 *          - putting wlan numbers in order,  
 *            automatic reconnection to router,
 *            bug fixes
 *            October 13, Bojan Jurca
 *            
 */


#ifndef __NETWORK__
  #define __NETWORK__

  // ----- includes, definitions and supporting functions -----

  #include <WiFi.h>
  #include <lwip/netif.h>
  #include <netif/etharp.h>
  #include <lwip/sockets.h>
  #include <esp_wifi.h>

  #include "file_system.h"  // network.h needs file_system.h to read configurations files

  // define the time in milli seconds ESP could be disconnected from router before trying to reconnect again
  #define CONNECTION_RETRY_PERIOD 3600000

  bool __retry_to_connect_if_disconnected__ = false;
  unsigned long __last_connection_retry_time__;

  String __compactNetworkConfiguration__ (String inp);
  String __insideBrackets__ (String inp, String opening, String closing);
  IPAddress IPAddressFromString (String ipAddress);
  String arp_a ();
  
  void __networkDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__ // use dmesg from telnet server if possible
      dmesg (message);
    #else
      Serial.println (message); 
    #endif
  }
  void (* networkDmesg) (String) = __networkDmesg__; // use this pointer to display / record system messages
  
  
  void connectNetwork () {                                        // connect to the network by calling this function
  
    // it is a little awkward why UNIX, LINUX, Raspbian are using so many network configuration files and how they are used

    String fileContent = "";

    // prepare configuration for network interface 2 that will be used latter to connect in STA-tion mode to WiFi (skip this if you don't want ESP32 to connect to your WiFi)
    readEntireFile (&fileContent, "/network/interfaces");
    if (fileContent == "") {
      Serial.printf ("[%10d] [network] /network/interfaces does not exist, creating new one ... ", millis ());
      
      fileContent =  "# only wlan0 can be used to connect to your WiFi\r\n"
                     "\r\n"
                     "# get IP address from DHCP\r\n"
                     "   iface wlan0 inet dhcp\r\n"                  // this method is preferable, you can configure your router to always get the same IP address
                     "\r\n"
                     "# use static IP address (example below)\r\n"   // comment upper line and uncomment this lines if you want to set a static IP address
                     "#   iface wlan0 inet static\r\n"
                     "#      address 10.0.0.3\r\n"                   // change 10.0.0.3 to the IP you want to assigne to your ESP32 in static mode
                     "#      netmask 255.255.255.0\r\n"              // change 255.255.255.0 to subnet mask you want to assigne to your ESP32 in static mode
                     "#      gateway 10.0.0.1\r\n";                  // change 10.0.0.1 to your router's IP

      if (writeEntireFile (fileContent, "/network/interfaces")) Serial.printf ("created.\n");
      else                                                      Serial.printf ("error creating /network/interfaces.\n");
    }

    // create /etc/wpa_supplicant.conf if it doesn't exist
    readEntireFile (&fileContent, "/etc/wpa_supplicant.conf");
    if (fileContent == "") {
      Serial.printf ("[%10d] [network] /etc/wpa_supplicant.conf does noes exist, creating new one ... ", millis ());
      
      fileContent = "network = {\r\n"
                    "   ssid = \"YOUR-STA-SSID\"\r\n"               // change YOUR-STA-SSID to your WiFi SSID here
                    "   psk  = \"YOUR-STA-PASSWORD\"\r\n"           // change YOUR-STA-PASSWORD to your WiFi password here
                    "}\r\n";

      if (writeEntireFile (fileContent, "/etc/wpa_supplicant.conf")) Serial.printf ("created.\n");
      else                                                           Serial.printf ("error creating /etc/wpa_supplicant.conf.\n");
    }

    // prepare configuration for network interface 1 that will be used latter to start WiFi access point (skip this if you don't want ESP32 to be an access point)
    // create /etc/wpa_supplicant.conf if it doesn't exist
    readEntireFile (&fileContent, "/etc/dhcpcd.conf");
    if (fileContent == "") {
      Serial.printf ("[%10d] [network] /etc/dhcpcd.conf does noes exist, creating new one ... ", millis ());

      fileContent =  "# only static IP addresses can be used for access point and only wlan1 can be used (example below)\r\n"
                     "\r\n"
                     "interface wlan1\r\n"
                     "   static ip_address = 10.0.1.3\r\n"           // change 10.0.1.3 to the access point IP your ESP32 will have here
                     "          netmask = 255.255.255.0\r\n"         // change 255.255.255.0 to the access point subnet mask your ESP32 will have here
                     "          gateway = 10.0.1.3\r\n";             // change 10.0.1.3 to the access point IP your ESP32 will have here
      
      if (writeEntireFile (fileContent, "/etc/dhcpcd.conf")) Serial.printf ("created.\n");
      else                                                   Serial.printf ("error creating /etc/dhcpcd.conf.\n");
    }

    // create /etc/hostapd/hostapd.conf if it doesn't exist
    readEntireFile (&fileContent, "/etc/hostapd/hostapd.conf");
    if (fileContent == "") {
      Serial.printf ("[%10d] [network] /etc/hostapd/hostapd.conf does noes exist, creating new one ... ", millis ());

      fileContent =  "# only wlan1 can be used for access point\r\n"
                     "\r\n"
                     "interface = wlan1\r\n"
                     "   ssid = ESP32_SRV\r\n"                      // change ESP32_SRV to your access point SSID here
                     "   # use at least 8 characters for wpa_passphrase\r\n"
                     "   wpa_passphrase = YOUR-AP-PASSWORD\r\n";    // change YOUR-AP-PASSWORD to your access point password here

      if (writeEntireFile (fileContent, "/etc/hostapd/hostapd.conf")) Serial.printf ("created.\n");
      else                                                            Serial.printf ("error creating /etc/hostapd/hostapd.conf.\n");
    }

    // read network configuration from configuration files and set it accordingly
    String staSSID = "";
    String staPassword = "";
    String staIP = "";
    String staSubnetMask = "";
    String staGateway = "";
  
    String apSSID = "";
    String apPassword = "";
    String apIP = "";
    String apSubnetMask = "";
    String apGateway = "";
    String s;
    int i;
    
    s = __insideBrackets__ (__compactNetworkConfiguration__ (readEntireTextFile ("/etc/wpa_supplicant.conf")), "network\n{", "}"); 
    staSSID     = __insideBrackets__ (s, "ssid ", "\n");
    staPassword = __insideBrackets__ (s, "psk ", "\n");
  
    s = __compactNetworkConfiguration__ (readEntireTextFile ("/network/interfaces") + "\n"); 
    i = s.indexOf ("iface wlan0 inet static");
    if (i < 0) i = s.indexOf ("iface wlan2 inet static"); // bacwards compatibility
    if (i >= 0) {
      s = s.substring (i);
      staIP         = __insideBrackets__ (s, "address ", "\n");
      staSubnetMask = __insideBrackets__ (s, "netmask ", "\n");
      staGateway    = __insideBrackets__ (s, "gateway ", "\n");
    }
  
    s = __compactNetworkConfiguration__ (readEntireTextFile ("/etc/hostapd/hostapd.conf") + "\n"); 
    i = s.indexOf ("interface wlan1");
    if (i >= 0) {
      s = s.substring (i);
      apSSID        = __insideBrackets__ (s.substring (i + 15), "ssid ", "\n");
      apPassword    = __insideBrackets__ (s.substring (i + 15), "wpa_passphrase ", "\n");
    }
  
    s = __compactNetworkConfiguration__ (readEntireTextFile ("/etc/dhcpcd.conf") + "\n"); 
    i = s.indexOf ("interface wlan1");
    if (i >= 0) {
      s = s.substring (i);
      apIP          = __insideBrackets__ (s.substring (i + 15), "static ip_address ", "\n");
      apSubnetMask  = __insideBrackets__ (s.substring (i + 15), "netmask ", "\n");
      apGateway     = __insideBrackets__ (s.substring (i + 15), "gateway ", "\n");
    }
  
    // connect STA and AP if defined
    WiFi.disconnect (true);
    WiFi.mode (WIFI_OFF);

    if (staSSID > "") { // setup STA
      if (staIP > "") { // configure static IP address
        Serial.printf ("[%10d] [network] [STA] connecting STAtion to router with static IP ...\n", millis ());
        WiFi.config (IPAddressFromString (staIP), IPAddressFromString (staGateway), IPAddressFromString (staSubnetMask));
      } else { // go with DHCP
        Serial.printf ("[%10d] [network] [STA] connecting STAtion to router through DHCP ...\n", millis ());
      }
      WiFi.begin (staSSID.c_str (), staPassword.c_str ());
      __last_connection_retry_time__ = millis ();
    }    
    if (apSSID > "") { // setup AP
        if (WiFi.softAP (apSSID.c_str (), apPassword.c_str ())) { 
          WiFi.softAPConfig (IPAddressFromString (apIP), IPAddressFromString (apGateway), IPAddressFromString (apSubnetMask));
          WiFi.begin ();
          Serial.printf ("[%10d] [network] [AP] SSID: %s\n", millis (), apSSID.c_str ());
          Serial.printf ("[%10d] [network] [AP] password: %s\n", millis (), apPassword.c_str ());
          Serial.printf ("[%10d] [network] [AP] IP: %s\n", millis (), WiFi.softAPIP ().toString ().c_str ());
        } else {
          // ESP.restart ();
          Serial.printf ("[%10d] [network] [AP] failed to initialize access point mode\n", millis ()); 
        }
    } 

    // set WiFi mode
    if (staSSID > "") { 
      if (apSSID > "") {
        WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
        Serial.printf ("[%10d] [WIFI_AP_STA]\n", millis ());
      } else {
        WiFi.mode (WIFI_STA); // only STA mode
        Serial.printf ("[%10d] [WIFI_STA]\n", millis ());
      }
    } else {
      if (apSSID > "") {
        WiFi.mode (WIFI_AP); // only AP mode
        Serial.printf ("[%10d] [WIFI_AP]\n", millis ());
      }
    }  


    // network event logging - see: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
    WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {
      static bool staStarted = false; // to prevent unneccessary messages
      switch (event) {
          case SYSTEM_EVENT_WIFI_READY:           // do not report this event - it is too frequent
                                                  // networkDmesg ("[network] WiFi interface ready.");
                                                  break;
          case SYSTEM_EVENT_SCAN_DONE:            networkDmesg ("[network] [STA] completed scan for access points.");
                                                  break;
          case SYSTEM_EVENT_STA_START:            if (!staStarted) {
                                                    staStarted = true;
                                                    networkDmesg ("[network] [STA] WiFi client started.");
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_STOP:             networkDmesg ("[network] [STA] WiFi clients stopped.");
                                                  break;
          case SYSTEM_EVENT_STA_CONNECTED:        networkDmesg ("[network] [STA] connected to WiFi " + WiFi.SSID () + ".");
                                                  break;
          case SYSTEM_EVENT_STA_DISCONNECTED:     if (staStarted) {
                                                    staStarted = false;
                                                    networkDmesg ("[network] [STA] disconnected from WiFi.");
                                                    __last_connection_retry_time__ = millis (); // this will force ESP to try to reconnect after CONNECTION_RETRY_PERIOD  
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  networkDmesg ("[network] [STA] authentication mode has changed.");
                                                  break;
          case SYSTEM_EVENT_STA_GOT_IP:           networkDmesg ("[network] [STA] obtained IP address: " + WiFi.localIP ().toString ());
                                                  break;
          case SYSTEM_EVENT_STA_LOST_IP:          networkDmesg ("[network] [STA] lost IP address and IP address is reset to 0.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:   networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): succeeded in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_FAILED:    networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): failed in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:   networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): timeout in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_PIN:       networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): pin code in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_AP_START:             networkDmesg ("[network] [AP] WiFi access point started.");
                                                  break;
          case SYSTEM_EVENT_AP_STOP:              networkDmesg ("[network] [AP] WiFi access point stopped.");
                                                  break;
          case SYSTEM_EVENT_AP_STACONNECTED:      networkDmesg ("[network] [AP] client connected.");
                                                  break;
          case SYSTEM_EVENT_AP_STADISCONNECTED:   networkDmesg ("[network] [AP] client disconnected.");
                                                  break;
          case SYSTEM_EVENT_AP_STAIPASSIGNED:     networkDmesg ("[network] [AP] assigned IP address to client.");
                                                  break;
          case SYSTEM_EVENT_AP_PROBEREQRECVED:    networkDmesg ("[network] [AP] received probe request.");
                                                  break;
          case SYSTEM_EVENT_GOT_IP6:              networkDmesg ("[network] IPv6 is preferred.");
                                                  break;
          case SYSTEM_EVENT_ETH_START:            networkDmesg ("[network] ethernet started.");
                                                  break;
          case SYSTEM_EVENT_ETH_STOP:             networkDmesg ("[network] ethernet stopped.");
                                                  break;
          case SYSTEM_EVENT_ETH_CONNECTED:        networkDmesg ("[network] ethernet connected.");
                                                  break;
          case SYSTEM_EVENT_ETH_DISCONNECTED:     networkDmesg ("[network] ethernet disconnected.");
                                                  break;
          case SYSTEM_EVENT_ETH_GOT_IP:           networkDmesg ("[network] ethernet obtained IP address.");
                                                  break;        
          default:                                networkDmesg ("[network] event: " + String (event)); // shouldn't happen
                                                  break;
      }
    });    
    
  }

  String __compactNetworkConfiguration__ (String inp) { // skips comments, ...
    String outp = "";
    bool inComment = false;  
    bool inQuotation = false;
    String c;
    bool lastCharacterIsSpace = false;
    for (int i = 0; i < inp.length (); i++) {
      c = inp.substring (i, i + 1);
  
           if (c == "#")                                        {inComment = true;}
      else if (c == "\"")                                       {inQuotation = !inQuotation;}
      else if (c == "\n")                                       {if (!outp.endsWith ("\n")) {if (!inQuotation && outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1); outp += "\n";} inComment = inQuotation = false;}
      else if (c == "{")                                        {if (!inComment) {while (outp.endsWith ("\n") || outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1); outp += "\n{\n";}}
      else if (c == "}")                                        {if (!inComment) {while (outp.endsWith ("\n") || outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1); outp += "\n}\n";}}    
      else if (c == " " || c == "\t" || c == "=" || c == "\r")  {if (!inComment && !outp.endsWith (" ") && !outp.endsWith ("\n")) outp += " ";}
      else if (!inComment) {outp += c;}
   
    }
    if (outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1);
    return outp;
  }
  
  String __insideBrackets__ (String inp, String opening, String closing) { // returns content inside of opening and closing brackets
    // Serial.println ("__insideBrackets__"); Serial.println (inp);
    int i = inp.indexOf (opening);
    if (i >= 0) {
      inp = inp.substring (i +  opening.length ());
      // Serial.println ("__insideBrackets__, opening"); Serial.println (inp);
      i = inp.indexOf (closing);
      if (i >= 0) {
        inp = inp.substring (0, i);
        // Serial.println ("__insideBrackets__, closing"); Serial.println (inp);
        return inp;
      }
    }
    return "";
  }

  String inet_ntoString (ip_addr_t addr) { // equivalent of inet_ntoa (struct in_addr addr) 
                                                  // inet_ntoa returns pointer to static string which may
                                                  // be a problem in multi-threaded environment
    return String (*(((byte *) &addr) + 0)) + "." + 
           String (*(((byte *) &addr) + 1)) + "." + 
           String (*(((byte *) &addr) + 2)) + "." + 
           String (*(((byte *) &addr) + 3));
  }
  
  IPAddress IPAddressFromString (String ipAddress) { // converts dotted IP address into IPAddress structure
    int ip1, ip2, ip3, ip4; 
    if (4 == sscanf (ipAddress.c_str (), "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
      return IPAddress (ip1, ip2, ip3, ip4);
    } else {
      Serial.printf ("[network] invalid IP address %s\n", ipAddress.c_str ());
      return IPAddress (0, 42, 42, 42); // == 1073421048 - invalid address - first byte of class A can not be 0
    }
  }

  String MacAddressAsString (byte *MacAddress, byte addressLength) {
    String s = "";
    char c [3];
    for (byte i = 0; i < addressLength; i++) {
      sprintf (c, "%02x", *(MacAddress ++));
      s += String (c);
      if (i < 5) s += ":";
    }
    return s;
  }

  // ----- ifconfig -----
  
  String ifconfig () {
    String s = "";
    struct netif *netif;
    for (netif = netif_list; netif; netif = netif->next) {
      if (netif_is_up (netif)) {
        if (s != "") s += "\r\n";
        s += String (netif->name [0]) + String (netif->name [1]) + String ((int) netif->name [2]) + "     hostname: " + (netif->hostname ? String (netif->hostname) : "") + "\r\n" + 
                 "        hwaddr: " + MacAddressAsString (netif->hwaddr, netif->hwaddr_len) + "\r\n" +
                 "        inet addr: " + inet_ntoString (netif->ip_addr) + "\r\n" + 
                 "        mtu: " + String (netif->mtu) + "\r\n";
      }
    }
    return s;    
  }
  
  String __appendString__ (String s, int toLenght) {
    while (s.length () < toLenght) s += " ";
    return s;
  }

  // ----- arp reference:  https://github.com/yarrick/lwip/blob/master/src/core/ipv4/etharp.c -----

  // first (re)make a definition of ARP table and get a pointer to it (not very elegant but I have no other idea how to get reference to arp table)
  enum etharp_state {
    ETHARP_STATE_EMPTY = 0,
    ETHARP_STATE_PENDING,
    ETHARP_STATE_STABLE,
    ETHARP_STATE_STABLE_REREQUESTING_1,
    ETHARP_STATE_STABLE_REREQUESTING_2
  #if ETHARP_SUPPORT_STATIC_ENTRIES
    ,ETHARP_STATE_STATIC
  #endif
  };
  
  struct etharp_entry {
  #if ARP_QUEUEING
    struct etharp_q_entry *q;
  #else
    struct pbuf *q;
  #endif
    ip4_addr_t ipaddr;
    struct netif *netif;
    struct eth_addr ethaddr;
    u16_t ctime;
    u8_t state;
  };

  struct etharp_entry *__getArpTablePointer__ () { 
    static etharp_entry *arpTablePointer = NULL;
  
    // if the pointer has been obtained once before we can just return it
    if (arpTablePointer) return arpTablePointer; // success
  
    // check if the first entry is stable and we can obtain pointer to it, then we can calculate pointer to ARP table
    ip4_addr_t *ipaddr;
    struct netif *netif;
    struct eth_addr *mac;
    if (etharp_get_entry (0, &ipaddr, &netif, &mac)) {
      networkDmesg ("[network] [ARP] got ARP table address.");
      byte offset = (byte *) &arpTablePointer->ipaddr - (byte *) arpTablePointer;
      return arpTablePointer = (struct etharp_entry *) ((byte *) ipaddr - offset); // success
    }
    
    // nothing worked, return failure
    return NULL; // failure
  }

  String arp_a () {
    struct etharp_entry *arpTablePointer = __getArpTablePointer__ ();

    struct netif *netif;
    ip4_addr_t *ipaddr;
    // scan ARP table it for each netif  
    String s = "";
    for (netif = netif_list; netif; netif = netif->next) {
      struct etharp_entry *p = arpTablePointer; // start scan of ARP table from the beginning with the next netif
      if (netif_is_up (netif)) {
        if (s != "") s += "\r\n\r\n";
        s += String (netif->name [0]) + String (netif->name [1]) + String ((int) netif->name [2]) + ": " + inet_ntoString (netif->ip_addr) + "\r\n  Internet Address      Physical Address      Type";
        if (arpTablePointer) { // we have got a valid pointer to ARP table
          for (int i = 0; i < ARP_TABLE_SIZE; i++) { // scan through ARP table
            if (p->state != ETHARP_STATE_EMPTY) {
              struct netif *arp_table_netif = p->netif; // make a copy of a pointer to netif in case arp_table entry is just beeing deleted
              if (arp_table_netif && arp_table_netif->num == netif->num) { // if ARP entry is for the same as netif we are displaying
                s += "\r\n  " + __appendString__ (inet_ntoString (*(ip_addr_t *) &p->ipaddr), 22) +
                     MacAddressAsString ((byte *) &p->ethaddr, 6) +  
                     (p->state > ETHARP_STATE_STABLE_REREQUESTING_2 ? "     static" : "     dynamic");
              } 
            }
            p ++;
          } // scan through ARP table
        } // we have got a valid pointer to ARP table
      }
    }
    return s + "\r\n";
  }  

  // periodicly check network status and reconnect if neccessary
  void network_doThings () {
    // try to get a pointer to ARP table if we haven't got it yet
    __getArpTablePointer__ ();

    // reconnect WiFi if neccessary
    if (__retry_to_connect_if_disconnected__ 
        && WiFi.status () != WL_CONNECTED 
        && millis () - __last_connection_retry_time__ > CONNECTION_RETRY_PERIOD) {

          networkDmesg ("[network] [STA] trying to reconnect.");
          WiFi.reconnect ();
          __last_connection_retry_time__ = millis ();
    }
  }

#endif
