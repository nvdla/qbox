// ================================================================
// NVDLA Open Source Project
// 
// Copyright(c) 2016 - 2017 NVIDIA Corporation.  Licensed under the
// NVDLA Open Hardware License; Check "LICENSE" which comes with 
// this distribution for more information.
// ================================================================

// File Name: qbox_aarch64.c

#include <tlm2c/tlm2c.h>
#include "qbox/qboxbase.h"

struct QBOXAARCH64
{
  QBOXBase *base;
};
typedef struct QBOXAARCH64 QBOXAARCH64;

QBOXAARCH64 aarch64_handle;

static void qbox_aarch64_init(void)
{
    qbox_add_aarch64_arguments(aarch64_handle.base);
    qbox_add_extra_arguments(aarch64_handle.base);
}

Model *tlm2c_elaboration(Environment *environment)
{
  /* Entry point called by the bridge. */
  tlm2c_set_environment(environment);

  aarch64_handle.base = qbox_base_init();

  qbox_aarch64_init();

  return (Model *)(qbox_get_handle());
}

