/*
 * qboxbase.c
 *
 * Copyright (C) 2014, GreenSocs Ltd.
 *
 * Developped by Konrad Frederic <fred.konrad@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking GreenSocs code, statically or dynamically with other modules
 * is making a combined work based on GreenSocs code. Thus, the terms and
 * conditions of the GNU General Public License cover the whole
 * combination.
 *
 * In addition, as a special exception, the copyright holders, GreenSocs
 * Ltd, give you permission to combine GreenSocs code with free software
 * programs or libraries that are released under the GNU LGPL, under the
 * OSCI license, under the OCP TLM Kit Research License Agreement or
 * under the OVP evaluation license.You may copy and distribute such a
 * system following the terms of the GNU GPL and the licenses of the
 * other code concerned.
 *
 * Note that people who make modified versions of GreenSocs code are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so. The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception.
 *
 */

#include "qbox/qboxbase.h"
#include <tlm2c/tlm2c.h>
#include "qemu/osdep.h" // Required by memory.h 
#include "qom/cpu.h"
#include "exec/memory.h"
#include "qemu/timer.h"

/*
 * MMIO Read/Write accesses which didn't reach an address space fall in the qbox
 * address space and call those callbacks.
 */
static uint64_t qbox_mmio_read(void *opaque, hwaddr addr, unsigned size);
static void qbox_mmio_write(void *opaque, hwaddr addr, uint64_t value,
                       unsigned size);
const MemoryRegionOps qbox_mem_ops = {
  .read = qbox_mmio_read,
  .write = qbox_mmio_write,
};

void iothread_init(int argc, char **argv, char **envp);

/*
 * Blocking transport for IRQ coming from tlm2c irq_socket.
 */
static void qbox_irq_b_transport(void *model, Payload *payload);

struct QBOXBase
{
  Model model;                    /*< TLM2C model.            */
  InitiatorSocket *master_socket; /*< Output for Memory.      */
  TargetSocket *irq_socket;       /*< Input for IRQs.         */
  GenericPayload *payload;

  Method *method;                 /*< Method for qbox_notify. */
  qemu_irq *irqs;
  size_t irqs_size;

  uint64_t quantum;               /*< Quantum time in ns.     */
  volatile bool inited;
  QEMUTimer *quantum_timer;
  bool exit;                      /*< QEMU request exit. */

  int argc;                       /*< Command line argument for this QBOX. */
  char **argv;
};

QBOXBase handle;

QBOXBase *qbox_get_handle(void)
{
  return &handle;
}

uint64_t qbox_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
  QBOXBase *qbox = opaque;

  if (!qbox->payload)
  {
    qbox->payload = payload_create();
  }

  payload_set_address(qbox->payload, addr);
  payload_set_size(qbox->payload, size);
  payload_set_command(qbox->payload, READ);
  b_transport(qbox->master_socket, (Payload *)qbox->payload);

  switch (payload_get_response_status(qbox->payload))
  {
    case ADDRESS_ERROR_RESPONSE:
      /*
       * In this case we do an unassigned access as it should be done if there
       * was no qbox.
       */
      return qbox_unassigned_mem_read(opaque, addr, size);
    break;
    case OK_RESPONSE:
      return payload_get_value(qbox->payload);
    break;
    default:
      fprintf(stderr, "error: unknown response.\n");
      abort();
    break;
  }
}

void qbox_mmio_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
  QBOXBase *qbox = opaque;

  if (!qbox->payload)
  {
    qbox->payload = payload_create();
  }

  payload_set_address(qbox->payload, addr);
  payload_set_size(qbox->payload, size);
  payload_set_value(qbox->payload, value);
  payload_set_command(qbox->payload, WRITE);
  b_transport(qbox->master_socket, (Payload *)qbox->payload);

  switch (payload_get_response_status(qbox->payload))
  {
    case ADDRESS_ERROR_RESPONSE:
      /*
       * In this case we do an unassigned access as it should be done if there
       * was no qbox.
       */
      return qbox_unassigned_mem_write(opaque, addr, value, size);
    break;
    case OK_RESPONSE:
      return;
    break;
    default:
      fprintf(stderr, "error: unknown response.\n");
      abort();
    break;
  }
}

void qbox_dmi_discovery(void)
{
  /*
   * DMI discovery and memory creation inside qemu.
   */
  char **params;
  size_t sz;
  size_t i;
  QBOXBase *qbox = &handle;
  bool dmi_ok = false;

  get_env()->get_param_list(NULL, &params, &sz);

  for (i = 0; i < sz; i++)
  {
    char *high_addr_name;
    char *base_addr_name;
    char *substr = strstr(params[i], "base_addr");
    char *module_name;
    if (substr) {
      module_name = strdup(params[i]);
      module_name[strlen(params[i]) - strlen(".base_addr")] = '\0';
      high_addr_name = strdup(params[i]);
      strcpy(&high_addr_name[strlen(params[i]) - strlen("base_addr")],
             "high_addr");
      base_addr_name = params[i];

      uint64_t base_addr = get_env()->get_uint_param(NULL, base_addr_name);
      uint64_t high_addr = get_env()->get_uint_param(NULL, high_addr_name);

      /*
       * Do a DMI transaction on the memory socket to check if it accept DMI.
       */
      DMIData dmi;
      if (!qbox->payload)
      {
        qbox->payload = payload_create();
      }

      payload_set_address(qbox->payload, base_addr);
      if (tlm2c_get_direct_mem_ptr(qbox->master_socket,
                                   (Payload *)qbox->payload, &dmi))
      {
        /*
         * The dmi pointer is valid!
         */
        memory_region_create_ram_ptr(module_name, high_addr - base_addr + 1,
                                     base_addr, dmi.pointer);
        dmi_ok = true;
      }
      free(module_name);
      free(high_addr_name);
    }
  }

  if (!dmi_ok) {
    fprintf(stderr, "no dmi region has been discovered.\n");
    abort();
  }
}

void qbox_export_irq(QBOXBase *qbox, qemu_irq *irq, size_t size)
{
  size_t i;
  if (qbox->irqs)
  {
    fprintf(stderr, "error: irq already exported.\n");
    abort();
  }

  qbox->irqs = (qemu_irq *)malloc(size * sizeof(qemu_irq));
  for (i = 0; i < size; i++)
  {
    qbox->irqs[i] = irq[i];
  }
  qbox->irqs_size = size;
}

#define IRQ_POOL_SIZE 128

typedef struct IRQTrigger {
  QBOXBase *qbox;
  size_t irq_line;
  uint64_t value;
  bool busy;
  bool allocated;
} IRQTrigger;

static IRQTrigger irq_pool[IRQ_POOL_SIZE];

static void qbox_trigger_an_irq(CPUState *cpu, run_on_cpu_data data)
{
  IRQTrigger *irq_node = (IRQTrigger *)data.host_ptr;

  qemu_mutex_lock_iothread();
  qemu_set_irq(irq_node->qbox->irqs[irq_node->irq_line],
               irq_node->value);

  if (irq_node->allocated) {
    free(irq_node);
  } else {
    irq_node->busy = false;
  }
  qemu_mutex_unlock_iothread();
}

static void qbox_irq_b_transport(void *model, Payload *payload)
{
  GenericPayload *p = (GenericPayload *)payload;
  QBOXBase *qbox = (QBOXBase *)model;
  size_t i;
  size_t irq_line = payload_get_address(p);
  uint64_t value = payload_get_value(p);
  IRQTrigger *irq_node = NULL;

  assert(irq_line < qbox->irqs_size);
  for (i = 0; i < IRQ_POOL_SIZE; i++) {
    /* There is a potential race condition here but it is okay as in the worse
     * case the next element will be choosen.
     */
    if (!irq_pool[i].busy) {
      irq_node = &irq_pool[i];
      break;
    }
  }
  if (!irq_node) {
    irq_node = (IRQTrigger *)malloc(sizeof(IRQTrigger));
    memset(irq_node, 0, sizeof(IRQTrigger));
    irq_node->allocated = true;
  }
  irq_node->busy = true;
  irq_node->irq_line = irq_line;
  irq_node->value = value;
  irq_node->qbox = qbox;

  async_safe_run_on_cpu(first_cpu, qbox_trigger_an_irq,
                        RUN_ON_CPU_HOST_PTR(irq_node));

  payload_set_response_status(p, OK_RESPONSE);
}

#if 0
/*
 * XXX: Maybe we can change the protocol/payload for this.
 */
static void qbox_irq_b_transport(void *model, Payload *payload)
{
  GenericPayload *p = (GenericPayload *)payload;
  QBOXBase *qbox = (QBOXBase *)model;

  size_t irq_line = payload_get_address(p);
  uint64_t value = payload_get_value(p);

  assert(irq_line < qbox->irqs_size);
  qemu_set_irq(qbox->irqs[irq_line], value);

  payload_set_response_status(p, OK_RESPONSE);
}
#endif

void qbox_add_argument(QBOXBase *qbox, const char *arg)
{
  if (arg != NULL) {
    qbox->argc++;
    qbox->argv = realloc(qbox->argv, qbox->argc * sizeof(char *));
    qbox->argv[qbox->argc - 1] = strdup(arg);
  }
}

/*
 * This is notified by the environment at the init.
 */
static void qbox_init_threads(Model *model)
{
  static int init = 0;
  QBOXBase *qbox = (QBOXBase *)model;

  qbox->quantum = get_env()->get_uint_param(NULL, "CPU.quantum");

  if (qbox->quantum == 0) {
    fprintf(stderr, "error: bad quantum...\n");
    abort();
  }

  if (!init) {
    qbox->inited = false;
    iothread_init(qbox->argc, qbox->argv, NULL);
  }
}

/*
 * TLM2C model related:
 *
 * - init the model.
 * - init the sockets.
 * - register the methods.
 */
QBOXBase *qbox_base_init(void)
{
  QBOXBase *qbox = &handle;
  model_init(&qbox->model, "qbox");
  qbox->master_socket = socket_initiator_create("memory_master");
  model_add_socket(&qbox->model, (Socket *)qbox->master_socket);

  qbox->irq_socket = socket_target_create("irq_slave");
  socket_target_register_b_transport(qbox->irq_socket, &qbox->model,
                                     qbox_irq_b_transport);
  model_add_socket(&qbox->model, (Socket *)qbox->irq_socket);

  qbox->method = method_create(qbox_init_threads, 1);
  model_add_method(&qbox->model, qbox->method);

  qbox->exit = false;

  qbox->argc = 0;
  qbox->argv = NULL;

  return &handle;
}

/*
 * This is the timer callback associated to qbox.
 * It is necessary that QEMU executes a given instruction then stops so we can
 * get them synchronized.
 *
 * This is achieved with the QEMU_CLOCK_VIRTUAL.
 */
static void quantum_expired(void *opaque)
{
  handle.inited = true;

  env_signal_end_of_quantum();
  /*
   * Program the next trigger in one quantum.
   */
  timer_mod_ns(handle.quantum_timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                                     + handle.quantum);
}

void qbox_init_quantum_timer(void)
{
  handle.quantum_timer =
    timer_new_ns(QEMU_CLOCK_VIRTUAL, quantum_expired, NULL);

  /* Call the callback the first time at 0. */
  quantum_expired(NULL);
}

void qbox_exit(void)
{
  printf("Exit QBOX.\n");
  handle.exit = true;
  env_request_stop();
}

void qbox_add_standard_arguments(QBOXBase *qbox)
{
  const char *argv[] = {
  "./toplevel",
  "-nographic",
  "-monitor",
  "/dev/null",
  "-icount",
  "1",
  "-s"};
  int argc = 7;
  int i;

  for (i = 0; i < argc; i++) {
    qbox_add_argument(qbox, argv[i]);
  }
}

void qbox_add_linux_arguments(QBOXBase *qbox)
{
  char *kernel = NULL;
  char *dtb = NULL;
  char *rootfs = NULL;
  char *kernel_cmd = NULL;

  get_env()->get_string_param(NULL, "CPU.kernel", &kernel);
  get_env()->get_string_param(NULL, "CPU.rootfs", &rootfs);
  get_env()->get_string_param(NULL, "CPU.dtb", &dtb);
  get_env()->get_string_param(NULL, "CPU.kernel_cmd", &kernel_cmd);

  if (kernel && (strlen(kernel) > 0)) {
    qbox_add_argument(qbox, "--kernel");
    qbox_add_argument(qbox, kernel);
  }

  if (rootfs && (strlen(rootfs) > 0)) {
    qbox_add_argument(qbox, "--initrd");
    qbox_add_argument(qbox, rootfs);
  }

  if (dtb && (strlen(dtb) > 0)) {
    qbox_add_argument(qbox, "--dtb");
    qbox_add_argument(qbox, dtb);
  }

  if (kernel_cmd && (strlen(kernel_cmd) > 0)) {
    qbox_add_argument(qbox, "--append");
    qbox_add_argument(qbox, kernel_cmd);
  }
}

void qbox_add_aarch64_arguments(QBOXBase *qbox)
{
  // default arguments
  qbox_add_argument(qbox, "./toplevel");
}

void qbox_add_extra_arguments(QBOXBase *qbox)
{
  char *extraArgumentsValue = NULL;
  char *tmpArgument = NULL;

  get_env()->get_string_param(NULL, "CPU.extra_arguments", &extraArgumentsValue);

  if (extraArgumentsValue && (strlen(extraArgumentsValue) > 0)) {
    tmpArgument = strtok(extraArgumentsValue, " ");
    while (tmpArgument != NULL) {
      qbox_add_argument(qbox, tmpArgument);
      tmpArgument = strtok(NULL, " ");
    }
  }
}
