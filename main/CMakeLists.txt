idf_component_register(SRCS "main.cpp"
           
                  INCLUDE_DIRS "."
                     
                    REQUIRES arduino arduino-audio-tools esp32-adp ArduinoJson esp32-camera SdFat mbedtls esp-tls)
