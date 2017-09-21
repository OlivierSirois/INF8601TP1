/*
 * dragon_tbb.c
 *
 *  Created on: 2011-08-17
 *      Author: Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#include <iostream>

extern "C" {
#include "dragon.h"
#include "color.h"
#include "utils.h"
}
#include "dragon_tbb.h"
#include "tbb/tbb.h"
#include "TidMap.h"

using namespace std;
using namespace tbb;

TidMap* tid;


class DragonLimits {
	public:
		DragonLimits ()
		{
			piece_init(&p);
		}
		DragonLimits (DragonLimits& d, sep)
		{
			piece_init(&p);
		}
		void operator() (const blocked_range<int>& r)
		{
			piece_limit(r.begin(), r.end(), &p);
		}
		void join (DragonLimits& j)
		{
			piece_merge(&p, j.p);
		}
		piece_t& getPiece ()
		{
			return p;
		}
		piece_t p;
};

class DragonDraw 
{
	public:
		DragonDraw(struct draw_data* parData)
		{
			data = parData;
			index = 0;
		}
		DragonDraw(const DragonDraw& d)
		{
			data = d.data;
		}
		void operator()(const blocked_range<uint64_t>& r) const
		{
			dragon_draw_raw(r.begin(), r.end(), data->dragon, data->dragon_width, data->dragon_height, data->limits, index);
		}
		struct draw_data* data;
		int index;
};

class DragonRender 
{
	public:
		DragonRender(struct draw_data* parData)
		{
			data = parData;
		}
		DragonRender(const DragonRender& d)
		{
			data = d.data;
			tid->getIdFromTid(gettid());
		}

		void operator()(const blocked_range<int>& r) const
		{
			scale_dragon(r.begin(), r.end(), data->image, data->image_width, data->image_height, data->dragon, data->dragon_width, data->dragon_height, data->palette);
		}
		struct draw_data* data;
};

class DragonClear {
public:
	DragonClear(char initVal, char *parCanvas)
	{
		value = initVal;
		canvas = parCanvas;
	}
	DragonClear(const DragonClear& d)
	{
		value = d.value;
		canvas = d.canvas;
	}	
	void operator()(const blocked_range<int>& r) const
	{
		init_canvas(r.begin(), r.end(), canvas, -1);
	}	
	char value;
	char *canvas;
};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread)
{
	//TODO("dragon_draw_tbb");

	struct draw_data data;
	limits_t limits;
	char *dragon = NULL;
	int scale_x;
	int scale_y;
	int scale;
	int deltaJ;
	int deltaI;
	int dragonWidth;
	int dragonHeight;
	int dragonSurface;

	tid = new MapTid(nb_thread);
	

	struct palette *palette = init_palette(nb_thread);
	if (palette == NULL)
		return -1;

	/* 1. Calculer les limites du dragon */
	dragon_limits_tbb(&limits, size, nb_thread);

	task_scheduler_init init(nb_thread);

	scale_x = dragonWidth / width + 1;
	scale_y = dragonWeight / height + 1;
	scale = (scale_x > scale_y ? scale_x : scale_y);
	deltaJ = (scale * width - dragonWidth) / 2;
	deltaI = (scale * height - dragonHeight) / 2;
	dragonWidth =limits.maximums.x - limits.minimums.x;
	dragonHeight = limits.maximums.y - limits.minimums.y;
	dragonSurface = dragonWidth * height;

	dragon = (char *) malloc(dragon_surface);
	if (dragon == NULL) {
		free_palette(palette);
		return -1;
	}

	data.nb_thread = nb_thread;
	data.dragon = dragon;
	data.image = image;
	data.size = size;
	data.image_height = height;
	data.image_width = width;
	data.dragon_width = dragon_width;
	data.dragon_height = dragon_height;
	data.limits = limits;
	data.scale = scale;
	data.deltaI = deltaI;
	data.deltaJ = deltaJ;
	data.palette = palette;
	data.tid = (int *) calloc(nb_thread, sizeof(int));

	/* 2. Initialiser la surface : DragonClear */

	DragonClear clear(-1, dragon);
	parallel_for(blocked_range<int>(0, dragon_surface), clear);

	/* 3. Dessiner le dragon : DragonDraw */
	
	DragonDraw draw(&data);
	parallel_for(blocked_range<uint64_t>(0, data.size), draw);

	/* 4. Effectuer le rendu final */

	DragonRender render(&data);
	parallel_for(blocked_range<int>(0, height), render);
	tid.dump();
	delete tid;
	
	init.terminate();

	free_palette(palette);
	FREE(data.tid);
	*canvas = dragon;
	return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread)
{
	//TODO("dragon_limits_tbb");
	DragonLimits limit;

	parallel_reduce(blocked_range<int>(0, size), limit);
	
	piece_t piece = limit.getPiece();
	*limits = piece.limits;

	return 0;
}
