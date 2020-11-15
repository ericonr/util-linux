/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/prctl.h>
#include <limits.h>

#include "caputils.h"
#include "pathnames.h"

static int test_cap(unsigned int cap)
{
	/* prctl returns 0 or 1 for valid caps, -1 otherwise */
	return prctl(PR_CAPBSET_READ, cap, 0, 0, 0) >= 0;
}

/* this cache makes cap_last_cap MT-unsafe */
static int last_cap = 0;

int cap_last_cap(void)
{
	/* cached last_cap value */
	if (last_cap) return last_cap;

	/* binary search over capabilities */
	/* starting with cap=INT_MAX means we always know that cap1 is invalid */
	unsigned int cap0 = 0, cap1 = INT_MAX, cap = INT_MAX;
	while (cap0 + 1 < cap1) {
		if (test_cap(cap)) {
			cap0 = cap;
		} else {
			cap1 = cap;
		}
		cap = (cap0 + cap1) / 2;
	}

	last_cap = cap;
	return last_cap;
}
