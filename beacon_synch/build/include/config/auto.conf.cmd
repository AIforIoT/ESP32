deps_config := \
	/home/miquel/esp/esp-idf/components/app_trace/Kconfig \
	/home/miquel/esp/esp-idf/components/aws_iot/Kconfig \
	/home/miquel/esp/esp-idf/components/bt/Kconfig \
	/home/miquel/esp/esp-idf/components/driver/Kconfig \
	/home/miquel/esp/esp-idf/components/esp32/Kconfig \
	/home/miquel/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/miquel/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/miquel/esp/esp-idf/components/ethernet/Kconfig \
	/home/miquel/esp/esp-idf/components/fatfs/Kconfig \
	/home/miquel/esp/esp-idf/components/freertos/Kconfig \
	/home/miquel/esp/esp-idf/components/heap/Kconfig \
	/home/miquel/esp/esp-idf/components/http_server/Kconfig \
	/home/miquel/esp/esp-idf/components/libsodium/Kconfig \
	/home/miquel/esp/esp-idf/components/log/Kconfig \
	/home/miquel/esp/esp-idf/components/lwip/Kconfig \
	/home/miquel/esp/esp-idf/components/mbedtls/Kconfig \
	/home/miquel/esp/esp-idf/components/mdns/Kconfig \
	/home/miquel/esp/esp-idf/components/mqtt/Kconfig \
	/home/miquel/esp/esp-idf/components/openssl/Kconfig \
	/home/miquel/esp/esp-idf/components/pthread/Kconfig \
	/home/miquel/esp/esp-idf/components/spi_flash/Kconfig \
	/home/miquel/esp/esp-idf/components/spiffs/Kconfig \
	/home/miquel/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/miquel/esp/esp-idf/components/vfs/Kconfig \
	/home/miquel/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/miquel/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/miquel/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/miquel/esp/esp-idf/examples/wifi/getting_started/station/main/Kconfig.projbuild \
	/home/miquel/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/miquel/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
