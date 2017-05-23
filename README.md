# MICO32
The MICO32 project is a version of "MiCO OS" that based on TI's CC3200 platform.

The project provide the basic "MICO Core APIs" `http://developer.mico.io/api`

The MICO32 project is a "MICO" environment port to CC3200

# Build the MICO32
## Prerequirements:
1. CC3200SDK installed (The project was tested CC3200SDK on 1.1.0)
2. TivaWare_C_Series-2.1.0.12573 installed
   The MICO32 project use some ipaddr api that came from Tiva's lwip port.
3. MICO 1.0 source (https://github.com/MXCHIP)

**Highlights**
1. mxchip's EasyLink re-implementation
2. LED status indicator on CC3200
