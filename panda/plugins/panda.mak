include $(BUILD_DIR)/config-host.mak
include $(BUILD_DIR)$(TARGET_DIR)config-devices.mak
include $(BUILD_DIR)$(TARGET_DIR)config-target.mak
include $(SRC_PATH)/rules.mak
#ifneq ($(HWDIR),)
#include $(HWDIR)/config.mak
#endif

PLUGIN_TARGET_DIR=$(BUILD_DIR)$(TARGET_DIR)panda/plugins

PLUGIN_SRC_ROOT=$(SRC_PATH)/panda/plugins
PLUGIN_SRC_DIR=$(PLUGIN_SRC_ROOT)/$(PLUGIN_NAME)

ifdef CONFIG_LINUX_USER

$(call set-vpath, $(SRC_PATH)/linux-user:$(SRC_PATH)/linux-user/$(TARGET_ABI_DIR))
QEMU_CFLAGS+=-I$(SRC_PATH)/linux-user/$(TARGET_ABI_DIR) -I$(SRC_PATH)/linux-user

endif

TARGET_PATH=$(SRC_PATH)/target-$(TARGET_BASE_ARCH)
QEMU_CFLAGS+=-I$(BUILD_DIR)/$(TARGET_DIR) -I$(TARGET_PATH) -I$(BUILD_DIR) -DNEED_CPU_H -fPIC
QEMU_CFLAGS+=$(GLIB_CFLAGS)

PLUGIN_OBJ_DIR=$(PLUGIN_TARGET_DIR)/panda/$(PLUGIN_NAME)

$(PLUGIN_OBJ_DIR):
	@[ -d  $@ ] || mkdir -p $@
