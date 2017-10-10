/*
 * qboxbase.h
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

#ifndef QBOXBASE_H
#define QBOXBASE_H

#include <stdio.h>
#include "hw/irq.h"

typedef struct QBOXBase QBOXBase;

/*
 * Init QBOX.
 */
QBOXBase *qbox_base_init(void);

/*
 * Get the current qbox handle.
 */
QBOXBase *qbox_get_handle(void);

/*
 * The platform must export CPU irqs to qbox so they can be bound to the irq
 * socket.
 */
void qbox_export_irq(QBOXBase *qbox, qemu_irq *irq, size_t size);

/**
 * Add an @argument to the @qbox command line.
 */
void qbox_add_argument(QBOXBase *qbox, const char *argument);

/**
 * Add all the standard arguments.
 */
void qbox_add_standard_arguments(QBOXBase *qbox);

/**
 * Add all linux related arguments (kernel, initrd, append, ..) according to the
 * environment parameters.
 */
void qbox_add_linux_arguments(QBOXBase *qbox);

/**
 * Add all aarch64 related arguments
 */
void qbox_add_aarch64_arguments(QBOXBase *qbox);

/*
 * Print all qbox command arguments
 */
void qbox_print_arguments(QBOXBase *qbox);

/*
 * Part of the init, discover the DMI memories.
 */
void qbox_dmi_discovery(void);

/*
 * Init the quantum timer.
 */
void qbox_init_quantum_timer(void);

/*
 * Exit qbox.
 */
void qbox_exit(void);

#endif /* !QBOXBASE_H */
