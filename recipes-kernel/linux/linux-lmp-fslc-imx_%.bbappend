DESCRIPTION = "kernel for Arduino Portenta MX8MM platform"

FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"

SRC_URI_append_portenta-mx8mm = " \
  file://portenta-mx8mm.cfg \
  file://0002-drivers-clk-rtc-enable-RTC-as-clock-input-for-the-pm.patch \
  file://0002-Extcon-GPIO_Add_DT_bindings.patch \
  file://1000-TEMP-FIX_PCIe_reset_after_clock.patch \
  file://mfd-bd718x7-wait-for-clock.patch \
  file://uart-imx-0001-Bug-rx-uart1-doesn-t-work-with-DMA-channels.patch \
"

SRC_URI_append_portenta-m8 = " \
  file://portenta-m8-standard.scc \
  file://portenta-m8.cfg \
"

SRC_URI_append_portenta-x8 = " \
  file://portenta-x8-standard.scc \
  file://portenta-x8.cfg \
  file://0001-rohm-bd718xx-add-dt-configurable-button-keycode.patch \
"

# file://rtc-pcf8563-remove-workaround.patch
