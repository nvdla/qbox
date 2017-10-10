// ================================================================
// NVDLA Open Source Project
// 
// Copyright(c) 2016 - 2017 NVIDIA Corporation.  Licensed under the
// NVDLA Open Hardware License; Check "LICENSE" which comes with 
// this distribution for more information.
// ================================================================

// File Name: nvdla.c

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"

#define TYPE_NVDLA "nvdla"
#define NVDLA(obj) OBJECT_CHECK(NVDLAState, (obj), TYPE_NVDLA)

typedef struct NVDLAState {
    SysBusDevice parent_obj;

    qemu_irq irq;

    bool int_level;

} NVDLAState;

static void nvdla_update(NVDLAState *s)
{
    qemu_set_irq(s->irq, s->int_level);
}

static void nvdla_interrupt(void * opaque, bool int_level)
{
    NVDLAState *s = (NVDLAState *)opaque;

    s->int_level = int_level;
    nvdla_update(s);
}

static void nvdla_init(Object *obj)
{
    NVDLAState *s = NVDLA(obj);
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(dev, &s->irq);
}

static const VMStateDescription vmstate_nvdla = {
    .name = "nvdla",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_BOOL(int_level, NVDLAState),
        VMSTATE_END_OF_LIST()
    }
};

static void nvdla_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_nvdla;
}

static const TypeInfo nvdla_info = {
    .name          = TYPE_NVDLA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NVDLAState),
    .instance_init = nvdla_init,
    .class_init    = nvdla_class_init,
};

static void nvdla_register_types(void)
{
    type_register_static(&nvdla_info);
}

type_init(nvdla_register_types)
