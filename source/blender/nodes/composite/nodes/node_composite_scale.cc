/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** Scale  ******************** */

namespace blender::nodes {

static void cmp_node_scale_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("X")).default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);
  b.add_input<decl::Float>(N_("Y")).default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composite_update_scale(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock;
  bool use_xy_scale = ELEM(node->custom1, CMP_SCALE_RELATIVE, CMP_SCALE_ABSOLUTE);

  /* Only show X/Y scale factor inputs for modes using them! */
  for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
    if (STR_ELEM(sock->name, "X", "Y")) {
      nodeSetSocketAvailability(ntree, sock, use_xy_scale);
    }
  }
}

void register_node_type_cmp_scale(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCALE, "Scale", NODE_CLASS_DISTORT, 0);
  ntype.declare = blender::nodes::cmp_node_scale_declare;
  node_type_update(&ntype, node_composite_update_scale);

  nodeRegisterType(&ntype);
}
