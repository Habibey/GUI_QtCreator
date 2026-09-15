#!/bin/bash
# GUI'yi (EHARPP) YARIŞMA GÜNÜ sabit IP planıyla başlatır -- bkz.
# Ebabil_EH_AI/CLAUDE.md. PC/Jetson/ET RPi kendi özel (internetsiz) Ethernet
# switch'ine şu sabit IP'lerle bağlı olmalı: PC=192.168.50.1,
# Jetson=192.168.50.2, ET RPi=192.168.50.3. DHCP/WiFi/.local'e bağımlı
# DEĞİL -- venue ağı ne olursa olsun bu üçü birbirini hep aynı adresten bulur.
#
# Kullanım: ./baslat_gui.sh
# Farklı bir ağda (ör. bugünkü gibi ev/ofis testinde) çalıştırman gerekirse
# bu scripti KULLANMA, EBABIL_JETSON_IP=<jetson_ip> ./build/EHARPP ile elle
# çalıştır.
set -e
cd "$(dirname "$0")/build"
EBABIL_JETSON_IP=192.168.50.2 ./EHARPP
