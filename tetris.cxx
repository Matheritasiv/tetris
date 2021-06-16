#include<cstdint>
#include<cstdlib>
#include<ctime>
#include<cmath>
#include<map>
#include<windows.h>
#include<windowsx.h>

using namespace std;

//{{{ Constants
const uint32_t red    = 0x0000FF;
const uint32_t orange = 0x007FFF;
const uint32_t yellow = 0x00BFBF;
const uint32_t green  = 0x007F00;
const uint32_t blue   = 0xFF0000;
const uint32_t cyan   = 0xBFBF00;
const uint32_t purple = 0x7F007F;
const uint32_t white  = 0xFFFFFF;
const uint32_t black  = 0x000000;
const uint32_t gray   = 0x3F3F3F;
const uint32_t gold   = 0x00D7FF;
const uint32_t lime   = 0x00FF00;
const uint32_t null   = (uint32_t)-1;

const unsigned int width  = 10;
const unsigned int height = 20;
const int unit            = 30;

const int border          = unit / 10;
const int client_width    = (width + 8) * unit;
const int client_height   = (height + 2) * unit;
//}}}

//{{{ Memory control
typedef void (*collector)(void *);
class gc {
public:
	gc(void *obj, collector fun) { cons(false, obj, fun); }
	gc(collector fun, void *obj) { cons(false, obj, fun); }
	gc(bool st, void *obj, collector fun) { cons(st, obj, fun); }
	~gc(void) {
		collector(object);
		if (dynamic && next_gc)
			delete next_gc;
	}
	static void garbage_collect_dynamic(void);
	static void garbage_collect_static(void);
	static void garbage_collect(void) {
		garbage_collect_dynamic();
		garbage_collect_static();
	}
private:
	static gc *allocated_objects;
	static gc *allocated_objects_static;
	void cons(bool, void *, collector);
	bool dynamic;
	void *object;
	collector collector;
	gc *next_gc;
};

gc *gc::allocated_objects;
gc *gc::allocated_objects_static;

void gc::cons(bool st, void *obj, ::collector fun) {
	object = obj;
	collector = fun;
	dynamic = !st;
	if (st) {
		next_gc = allocated_objects_static;
		allocated_objects_static = this;
	} else {
		next_gc = allocated_objects;
		allocated_objects = this;
	}
}

void gc::garbage_collect_dynamic(void) {
	if (allocated_objects) {
		delete allocated_objects;
		allocated_objects = NULL;
	}
}

void gc::garbage_collect_static(void) {
	for (gc *ptr = allocated_objects_static; ptr; ptr = ptr->next_gc)
		ptr->collector(ptr->object);
}
//}}}
//{{{ Graphic control
HWND window;
HINSTANCE instance;
HDC hdc_main, hdc_sub;
typedef enum { normal, translucent, dim } draw_type;
const int num_type= 3;
//{{{ Color operation
uint32_t color_mix(uint32_t color_bg, uint32_t color, double alpha) {
	if (alpha < 0)
		alpha = 0;
	else if (alpha > 1)
		alpha = 1;
	uint32_t c = 0;
	c |= (uint8_t)((color_bg >> 16 & 0xFF) * (1 - alpha) +\
		(color >> 16 & 0xFF) * alpha);
	c <<= 8;
	c |= (uint8_t)((color_bg >> 8 & 0xFF) * (1 - alpha) +\
		(color >> 8 & 0xFF) * alpha);
	c <<= 8;
	c |= (uint8_t)((color_bg & 0xFF) * (1 - alpha) +\
		(color & 0xFF) * alpha);
	return c;
}

uint32_t color_saturation(uint32_t color, double alpha) {
// We use HSL model's saturation
	if (alpha < 0)
		alpha = 0;
	uint8_t r = color & 0xFF;
	uint8_t g = color >> 8 & 0xFF;
	uint8_t b = color >> 16 & 0xFF;
	uint8_t *max, *min, *mid;
	if (r < g)
		max = &g, min = &r;
	else
		max = &r, min = &g;
	if (*max < b)
		mid = max, max = &b;
	else if (*min > b)
		mid = min, min = &b;
	else
		mid = &b;
	if (*max == *min)
		return color;
	double cc1, c1 = ((double)(*min) + .5) / 256;
	double c2 = ((double)(*mid) + .5) / 256;
	double cc3, c3 = ((double)(*max) + .5) / 256;
	double l = (c1 + c3) / 2;
	if (alpha > 1) {
		if (l < .5) {
			if ((l - c1) * alpha > l - 1./512)
				alpha = (l - 1./512) / (l - c1);
		} else {
			if ((c3 - l) * alpha > 1-1./512 - l)
				alpha = (1-1./512 - l) / (c3 - l);
		}
	}
	*min = (uint8_t)(256 * (cc1 = l + (c1 - l) * alpha));
	*max = (uint8_t)(256 * (cc3 = l + (c3 - l) * alpha));
	*mid = (uint8_t)(256 * (c2 < l ? cc3 - (c3 - c2) * alpha :\
		cc1 + (c2 - c1) * alpha));
	return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16;
}
//}}}
//{{{ Draw a 3D frame
void draw_frame(HDC hdc, int pos_x, int pos_y, int width, int height, int border,\
	uint32_t color, uint32_t color_u, uint32_t color_d) {
	RECT r1, r2;
	if (border * 2 > width || border * 2 > height)
		return;
	if (border > 0) {
		HBRUSH brush_u = CreateSolidBrush(color_u);
		HBRUSH brush_d = CreateSolidBrush(color_d);
		HPEN pen = CreatePen(PS_SOLID, 1, color_mix(color_u, color_d, .5));
		HPEN old_pen = (HPEN)SelectObject(hdc, pen);
		POINT p[3];
		p[1].x = r1.left = (p[0].x = p[2].x = r2.left = pos_x) + border;
		r2.right = (r1.right = pos_x + width) - border;
		p[1].y = p[2].y = r2.top = (p[0].y = r2.bottom = pos_y + height) - border;
		r1.bottom = (r1.top = pos_y) + border;
		FillRect(hdc, &r1, brush_u);
		FillRect(hdc, &r2, brush_d);
		HRGN region = CreatePolygonRgn(p, 3, ALTERNATE);
		FillRgn(hdc, region, brush_u);
		DeleteObject(region);
		p[0].y--; p[1].y--;
		Polyline(hdc, p, 2);
		int t = r1.right;
		r1.right = r1.left;
		r1.left = r2.left;
		p[1].x = r2.left = r2.right;
		p[0].x = p[2].x = r2.right = t;
		t = r1.bottom;
		r1.bottom = r2.top;
		p[1].y = p[2].y = r2.top = t;
		p[0].y = r1.top;
		FillRect(hdc, &r1, brush_u);
		FillRect(hdc, &r2, brush_d);
		region = CreatePolygonRgn(p, 3, ALTERNATE);
		FillRgn(hdc, region, brush_d);
		DeleteObject(region);
		p[0].x--; p[1].x--;
		Polyline(hdc, p, 2);
		SelectObject(hdc, old_pen);
		DeleteObject(pen);
		DeleteObject(brush_u);
		DeleteObject(brush_d);
		r1.left = r1.right;
		r1.right = r2.left;
		r1.top = r2.top;
	} else {
		r1.left = pos_x;
		r1.right = pos_x + width;
		r1.top = pos_y;
		r1.bottom = pos_y + height;
	}
	HBRUSH brush = CreateSolidBrush(color);
	FillRect(hdc, &r1, brush);
	DeleteObject(brush);
}

inline void fill_bg(HDC hdc, int width, int height, uint32_t color) {
	draw_frame(hdc, 0, 0, width, height, 0, color, 0, 0);
}
inline void fill_bg(HDC hdc, int width, int height) {
	fill_bg(hdc, width, height, black);
}
//}}}
//{{{ New bitmap
HBITMAP new_bitmap(int width, int height) {
	HDC hdc = GetDC(window);
	HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);
	ReleaseDC(window, hdc);
	return bmp;
}
//}}}
//{{{ Font control
extern unsigned char ascii_16[] asm ("_binary_ASC16_start");
extern unsigned char ascii_24[] asm ("_binary_ASC24_start");

void put_char(HDC hdc, bool big, bool slant, int scale,\
	int pos_x, int pos_y, uint8_t c, uint32_t color) {
	const int slant_ratio = 5;
	bool mirror = c & 0x80;
	int i, j;
	c &= 0x7F;
	if (big) {
		uint16_t *ptr, t;
		int k = 24 * scale;
		int mask = 8 - mirror;
		ptr = (uint16_t *)(ascii_24 + c * 48);
		for (j = 0; j < 24; j++, k -= scale, ptr++)
		for (t = 0x8000, i = 0; i < 16; i++, t >>= 1)
		if (*ptr & t) {
			int x = pos_x + (i ^ mask) * scale;
			int y = pos_y + j * scale;
			int offset = k;
			for (int j = 0; j < scale; j++, offset--)
			for (int i = 0; i < scale; i++)
				SetPixel(hdc,\
					x + (slant ? offset / slant_ratio : 0) + i,\
					y + j, color);
		}
	} else {
		uint8_t *ptr, t;
		int k = 16 * scale;
		int mask = mirror ? 7 : 0;
		ptr = ascii_16 + c * 16;
		for (j = 0; j < 16; j++, k -= scale, ptr++)
		for (t = 0x80, i = 0; i < 8; i++, t >>= 1)
		if (*ptr & t) {
			int x = pos_x + (i ^ mask) * scale;
			int y = pos_y + j * scale;
			int offset = k;
			for (int j = 0; j < scale; j++, offset--)
			for (int i = 0; i < scale; i++)
				SetPixel(hdc,\
					x + (slant ? offset / slant_ratio : 0) + i,\
					y + j, color);
		}
	}
}

void put_string(HDC hdc, bool big, bool slant, int scale,\
	int pos_x, int pos_y, const char *s, int len, int space, uint32_t color) {
	if (scale <= 0)
		return;
	int i, x, step = scale * (big ? 16 : 8) + space;
	for (i = 0, x = pos_x; (len == -1 || i < len) && s[i]; x += step, i++)
		put_char(hdc, big, slant, scale, x, pos_y, s[i], color);
}
//}}}
//}}}
//{{{ Timer control
typedef enum { timer_drop, timer_lock, timer_animation } timer_id;
const int num_timer = 3;
bool clear_timer_queue = false;
void kill_all_timer(void);

void set_timer(timer_id timer, int delay) {
	static clock_t start[num_timer];
	static int remain[num_timer];
	static gc *dummy = new gc(NULL, [] (void *) {
		kill_all_timer();
	});
	if (delay > 0) {
		start[timer] = clock();
		remain[timer] = delay;
		SetTimer(window, timer + 1, delay, NULL);
	} else if (delay == -2) {
		remain[timer] = 0;
		start[timer] = (clock_t)0;
		KillTimer(window, timer + 1);
	} else if (delay == -1) {
		if (start[timer] != (clock_t)0) {
			if ((remain[timer] -=\
				(clock() - start[timer]) /\
				(CLOCKS_PER_SEC / 1000)) <= 0)
				remain[timer] = 1;
			start[timer] = (clock_t)0;
			KillTimer(window, timer + 1);
		}
	} else {
		if (start[timer] == (clock_t)0 && remain[timer] > 0) {
			start[timer] = clock();
			SetTimer(window, timer + 1, remain[timer], NULL);
		}
	}
}

inline void kill_timer(timer_id timer) {
	set_timer(timer, -2);
}
inline void kill_all_timer(void) {
	for (int i = 0; i < num_timer; i++)
		kill_timer((timer_id)i);
}
inline void stop_timer(timer_id timer) {
	set_timer(timer, -1);
}
inline void resume_timer(timer_id timer) {
	set_timer(timer, 0);
}
//}}}

//{{{ Definition of board
//{{{   Base class of board
class board {
public:
	board(int x, int y, int w, int h) { init_board(x, y, w, h); }
	board(const char *title, int x, int y, int w, int h) {
		init_board(x, y, w, h);
		init_title(title);
	}
	~board(void) { if (saved_bmp) DeleteObject(saved_bmp); }
	void draw_bitmap(HBITMAP, int, int, int, int, int, int);
	void draw_bitmap(HBITMAP bmp, int x, int y, int w, int h) {
		draw_bitmap(bmp, 0, 0, x, y, w, h);
	}
	void draw_bitmap(HBITMAP bmp, int w, int h) {
		draw_bitmap(bmp, 0, 0, w, h);
	}
	void save_bitmap(void);
	void restore_bitmap(void);
	void flush_board(bool, RECT *);
	void flush_board(RECT *area) { flush_board(true, area); }
	void flush_board(bool erase) { flush_board(erase, NULL); }
	void flush_board(void) { flush_board((RECT *)NULL); }
	int get_width(void) { return width; }
	int get_height(void) { return height; }
protected:
	int pos_x, pos_y;
	int width, height;
private:
	void init_board(int, int, int, int);
	void init_title(const char *);
	RECT rect;
	HBITMAP saved_bmp = NULL;
};
//{{{ Initialize board and title
void board::init_board(int x, int y, int w, int h) {
	rect.left = pos_x = x;
	rect.top = pos_y = y;
	rect.right = x + (width = w);
	rect.bottom = y + (height = h);
	draw_frame(hdc_main, x - border, y - border,\
		w + 2 * border, h + 2 * border, border, black,\
		color_mix(black, gray, .5), color_mix(white, gray, .5));
}
void board::init_title(const char *title) {
	put_string(hdc_main, true, false, 1,\
		pos_x + (width - strlen(title) * (16 - 3)) / 2,\
		pos_y - 3 * border - 24, title, -1, -3, white);
}
//}}}
//{{{ Draw a bitmap to board area with truncation
void board::draw_bitmap(HBITMAP bmp, int dx, int dy, int x, int y, int w, int h) {
	if (x < 0)
		dx -= x, w += x, x = 0;
	if (y < 0)
		dy -= y, h += y, y = 0;
	if (w > width - x)
		w = width - x;
	if (h > height - y)
		h = height - y;
	if (w <= 0 || h <= 0)
		return;
	HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp);
	BitBlt(hdc_main, pos_x + x, pos_y + y, w, h, hdc_sub, dx, dy, SRCCOPY);
	SelectObject(hdc_sub, old_bmp);
	RECT area;
	area.right = (area.left = x) + w;
	area.bottom = (area.top = y) + h;
	flush_board(false, &area);
}
//}}}
//{{{ Save/Restore content of board area
void board::save_bitmap(void) {
	if (!saved_bmp) {
		saved_bmp = new_bitmap(width, height);
	}
	HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, saved_bmp);
	BitBlt(hdc_sub, 0, 0, width, height, hdc_main, pos_x, pos_y, SRCCOPY);
	SelectObject(hdc_sub, old_bmp);
}
void board::restore_bitmap(void) {
	if (saved_bmp) {
		draw_bitmap(saved_bmp, width, height);
		flush_board(false, NULL);
	}
}
//}}}
//{{{ Flush board with given area
void board::flush_board(bool erase, RECT *area) {
	if (!area) {
		area = &rect;
	} else {
		if (area->left < 0)
			area->left = 0;
		if (area->top < 0)
			area->top = 0;
		if (area->right > width)
			area->right = width;
		if (area->bottom > height)
			area->bottom = height;
		if (area->left >= area->right || area->top >= area->bottom)
			return;
		area->left += pos_x;
		area->right += pos_x;
		area->top += pos_y;
		area->bottom += pos_y;
	}
	if (erase)
		FillRect(hdc_main, area, (HBRUSH)GetStockObject(BLACK_BRUSH));
	InvalidateRect(window, area, false);
}
//}}}
//}}}
//{{{   Derived class of game board
class game_board : public board {
public:
	game_board(int x, int y, int w, int h) : board(x, y, w * unit, h * unit) {}
	game_board(const char *title, int x, int y, int w, int h) :\
		board(title, x, y, w * unit, h * unit) {}
	void show_block(int, int, int, int, uint32_t, draw_type);
	void show_block(int, int, uint32_t, draw_type);
	void erase_block(int, int, int, int);
	void erase_block(int, int);
	void erase_lines(int, int);
	void move_line(int, int);
};
void game_board_collector(void *p) {
	delete (game_board *)p;
}
//{{{   Show a block
void game_board::show_block(int offset_x, int offset_y, int index_x, int index_y,\
	uint32_t color, draw_type type) {
	static map<uint32_t, HBITMAP> stored_bmp[num_type];
	static gc dummy(true, stored_bmp, [] (void *p) {
		map<uint32_t, HBITMAP>* array = (map<uint32_t, HBITMAP>*)p;
		for (int i = 0; i < num_type; i++) {
			for (auto e : array[i])
				DeleteObject(e.second);
			array[i].clear();
		}
	});
	map<uint32_t, HBITMAP>::iterator element;
	HBITMAP block;
	if ((element = stored_bmp[type].find(color)) == stored_bmp[type].end()) {
		uint32_t color_c, color_u, color_d;
		color_u = color_mix(white, color, .5);
		color_d = color_mix(black, color, .5);
		switch (type) {
		case normal:
			color_c = color;
			break;
		case translucent:
			color_c = color_mix(black, color, .3);
			color_u = color_mix(black, color_u, .3);
			color_d = color_mix(black, color_d, .3);
			break;
		case dim:
			color_c = color_saturation(color, .7);
			color_u = color_saturation(color_u, .7);
			color_d = color_saturation(color_d, .7);
			break;
		}
		block = new_bitmap(unit, unit);
		HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, block);
		draw_frame(hdc_sub, 0, 0, unit, unit, unit / 10,\
			color_c, color_u, color_d);
		SelectObject(hdc_sub, old_bmp);
		stored_bmp[type][color] = block;
	} else {
		block = element->second;
	}
	draw_bitmap(block,\
		offset_x + index_x * unit,\
		height - offset_y - (index_y + 1) * unit,\
		unit, unit);
}

inline void game_board::show_block(int index_x, int index_y, uint32_t color, draw_type type) {
	show_block(0, 0, index_x, index_y, color, type);
}
//}}}
//{{{   Erase a block
void game_board::erase_block(int offset_x, int offset_y, int index_x, int index_y) {
	RECT area;
	area.right = (area.left = offset_x + index_x * unit) + unit;
	area.bottom = (area.top = height - offset_y - (index_y + 1) * unit) + unit;
	flush_board(&area);
}

void game_board::erase_block(int index_x, int index_y) {
	erase_block(0, 0, index_x, index_y);
}
//}}}
//{{{   Erase lines
void game_board::erase_lines(int start, int end) {
	RECT area;
	area.left = 0;
	area.right = width;
	area.top = height - (end + 1) * unit;
	area.bottom = height - start * unit;
	flush_board(&area);
}
//}}}
//{{{   Move line src to line dst
void game_board::move_line(int dst, int src) {
	if (src == -1) {
		erase_lines(dst, dst);
	} else if (src >= 0 && src * unit < height && dst >= 0 && dst * unit < height) {
		BitBlt(hdc_main, pos_x, pos_y + height - (dst + 1) * unit, width, unit,\
			hdc_main, pos_x, pos_y + height - (src + 1) * unit, SRCCOPY);
		RECT area;
		area.left = 0;
		area.right = width;
		area.top = (area.bottom = height - dst * unit) - unit;
		flush_board(false, &area);
	}
}
//}}}
//}}}
//{{{   Derived class of stat board
class stat_board : public board {
public:
	stat_board(int x, int y) : board(x, y, 150, 30) {}
	stat_board(const char *title, int x, int y) : board(title, x, y, 150, 30) {}
	void show_digit(int);
};
void stat_board_collector(void *p) {
	delete (stat_board *)p;
}
//{{{   Show a digit
void stat_board::show_digit(int digit) {
	const int max_digit = 1000000000;
	static HBITMAP stored_num[10];
	static gc dummy(true, stored_num, [] (void *p) {
		HBITMAP* array = (HBITMAP*)p;
		for (int i = 0; i < 10; i++)
		if (array[i])
			DeleteObject(array[i]);
	});
	if (digit < 0)
		digit = digit % max_digit + max_digit;
	else if (digit >= max_digit)
		digit %= max_digit;
	HBITMAP num;
	for (int i = 8; i >= 0; digit /= 10, i--) {
		uint8_t d = digit % 10;
		if (!(num = stored_num[d])) {
			num = new_bitmap(16, 30);
			HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, num);
			fill_bg(hdc_sub, 16, 30);
			put_char(hdc_sub, false, false, 2, 0, 1, '0' + d, lime);
			SelectObject(hdc_sub, old_bmp);
			stored_num[d] = num;
		}
		draw_bitmap(num, 4 + 16 * i, 0, 16, 30);
	}
}
//}}}
//}}}
//}}}

//{{{ UI control -- Part I
game_board *main_board, *next_board;
stat_board *level_board, *score_board, *lines_board;
//{{{ Animation
void (*animation)(void);
void (*continuation)(void);
inline void stop_animation(void) { animation = continuation = NULL; }
//{{{ Default animation end
void default_animation_end(void) {
	animation = NULL;
	if (continuation) {
		void (*cont)(void) = continuation;
		continuation = NULL;
		cont();
	}
}
//}}}
//{{{ Show various languages
void show_language(void) {
	static int frame, offset;
	static HBITMAP bmp = LoadBitmap(instance, MAKEINTRESOURCE(200));
	static gc *dummy = new gc(bmp, (collector)DeleteObject);
	const int num = 13;
	const int w = 150;
	const int h = 30;
	const int8_t fix[] = { 0, 0, 0, 0, 0, 0, 5, 5, 0, 5, 0, -5, 0 };
	int wd = next_board->get_width();
	int ht = next_board->get_height();
	int x = (wd - w) / 2;
	int y = (ht - h) / 2;
	if (!animation) {
		frame = offset = 0;
		animation = show_language;
	}
	if (frame >= num)
		frame = offset = 0;
	next_board->flush_board();
	next_board->draw_bitmap(bmp, 0, offset, x, y - fix[frame] / 2, w, h + fix[frame]);
	offset += h + fix[frame++];
	set_timer(timer_animation, 2000);
}
//}}}
//{{{ Ready Go!
void ready_go(void) {
	static int frame;
	HBITMAP bmp, old_bmp;
	const char *str[] = { "READY", "GO!" };
	const int delay[] = { 1000, 500 };
	const int size = 3;
	const int h = 16 * size;
	int wd = main_board->get_width();
	int ht = main_board->get_height();
	int y = ht / 5 - h / 2;
	if (!animation) {
		frame = 0;
		animation = ready_go;
		main_board->flush_board();
	}
	if (frame < sizeof(str) / sizeof(str[0])) {
		bmp = new_bitmap(wd, h);
		old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp);
		fill_bg(hdc_sub, wd, h);
		put_string(hdc_sub, false, false, size,\
			(wd - strlen(str[frame]) * 8 * size) / 2, 0,\
			str[frame], -1, 0, white);
		SelectObject(hdc_sub, old_bmp);
		main_board->draw_bitmap(bmp, 0, y, wd, h);
		DeleteObject(bmp);
		set_timer(timer_animation, delay[frame++]);
	} else {
		main_board->flush_board();
		default_animation_end();
	}
}
//}}}
//}}}
//{{{ Initialize game board
void init_board(void) {
	fill_bg(hdc_main, client_width, client_height, gray);
	new gc(game_board_collector,
	main_board = new game_board(unit, unit, width, height));
	int x = unit * (width + 2);
	int y = unit + 30;
	int size = 5;
	new gc(game_board_collector,
	next_board = new game_board("NEXT", x, y, size, size));
	new gc(stat_board_collector,
	level_board = new stat_board("LEVEL", x, y + unit * size + 60));
	new gc(stat_board_collector,
	score_board = new stat_board("SCORE", x, y + unit * size + 150));
	new gc(stat_board_collector,
	lines_board = new stat_board("LINES", x, y + unit * size + 240));
	level_board->show_digit(0);
	score_board->show_digit(0);
	lines_board->show_digit(0);
}
//}}}
//{{{ Welcome screen
void welcome(void) {
	const char *title = "TET\xD2IS";
	const char *fun = "From Russia with Fun!";
	const uint32_t logo[6][7] = {
		{ null, red, red, red, red, green, null },
		{ yellow, yellow, blue, blue, blue, green, green },
		{ yellow, yellow, blue, orange, orange, orange, green },
		{ null, cyan, cyan, cyan, purple, orange, null },
		{ null, null, cyan, purple, purple, null, null },
		{ null, null, null, purple, null, null, null }
	};
	const int space = 8;
	const int size = 5;
	const int word = -4;
	int wd = main_board->get_width();
	int ht = main_board->get_height();
	int x = (wd - strlen(title) * (8 * size + space) + space) / 2;
	int y = (ht - 16 * size) / 5;
	HBITMAP bmp = new_bitmap(wd, ht);
	HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp);
	fill_bg(hdc_sub, wd, ht);
	put_string(hdc_sub, false, false, size,\
		x + 2, y + 1, title, -1, space, color_mix(black, gold, .5));
	put_string(hdc_sub, false, false, size,\
		x - 1, y - 1, title, -1, space, gold);
	put_string(hdc_sub, true, true, 1,\
		(wd - strlen(fun) * (16 + word)) / 2,\
		(ht - 24) / 2, fun, -1, word, white);
	SelectObject(hdc_sub, old_bmp);
	main_board->draw_bitmap(bmp, wd, ht);
	DeleteObject(bmp);
	for (int j = 0; j < 6; j++)
	for (int i = 0; i < 7; i++)
	if (logo[j][i] != (uint32_t)-1)
		main_board->show_block((wd - unit *  7) / 2, 0, i, j, logo[j][i], normal);
	show_language();
}
//}}}
//{{{ Clear lines
void clear_line(int *clear, int top_line) {
	static int *static_clear, static_top_line;
	static void (*ani_clear_line)(void) = [] (void) {
		static int frame;
		static int num_clear;
		static int clear[5];
		if (!animation) {
			int i;
			frame = (width + 1) / 2;
			animation = ani_clear_line;
			for (i = 0; static_clear[i] < height; i++)
				clear[i] = static_clear[i];
			clear[num_clear = i] = height;
		}
		if (--frame >= 0) {
			for (int i = 0; i < num_clear; i++) {
				main_board->erase_block(frame, clear[i]);
				main_board->erase_block(width - 1 - frame, clear[i]);
			}
			set_timer(timer_animation, 100);
		} else {
			int i, y, y0;
			y = y0 = clear[0];
			i = 1;
			while (y < static_top_line) {
				if (++y < clear[i])
					main_board->move_line(y0++, y);
				else
					i++;
			}
			main_board->erase_lines(y0, static_top_line);
			default_animation_end();
		}
	};
	static_clear = clear;
	static_top_line = top_line;
	ani_clear_line();
}
//}}}
//{{{ Draw pause frame
void draw_pause(void) {
	HBITMAP bmp, old_bmp;
	const char *title = "PAUSE";
	const int size = 3;
	const int h = 16 * size;
	int wd = main_board->get_width();
	int ht = main_board->get_height();
	int y = ht / 5 - h / 2;
	bmp = new_bitmap(wd, h);
	old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp);
	fill_bg(hdc_sub, wd, h);
	put_string(hdc_sub, false, false, size,\
		(wd - strlen(title) * 8 * size) / 2, 0,\
		title, -1, 0, white);
	SelectObject(hdc_sub, old_bmp);
	main_board->flush_board();
	main_board->draw_bitmap(bmp, 0, y, wd, h);
	DeleteObject(bmp);
}
//}}}
//{{{ Draw help frame
void draw_help(void) {
	static int frame, offset;
	static HBITMAP bmp = LoadBitmap(instance, MAKEINTRESOURCE(201));
	static gc *dummy = new gc(bmp, (collector)DeleteObject);
	const int w = 300;
	const int h = 600;
	int wd = main_board->get_width();
	int ht = main_board->get_height();
	int x = (wd - w) / 2;
	int y = (ht - h) / 2;
	main_board->flush_board();
	main_board->draw_bitmap(bmp, 0, 0, x, y, w, h);
}
//}}}
//{{{ Block operation used by game kernel
inline void draw_block(int x, int y, uint32_t color, draw_type type) {
	main_board->show_block(x, y, color, type);
}
inline void sweep_block(int x, int y, uint32_t) {
	main_board->erase_block(x, y);
}
inline void draw_block_normal(int x, int y, uint32_t color) {
	draw_block(x, y, color, normal);
}
inline void draw_block_translucent(int x, int y, uint32_t color) {
	draw_block(x, y, color, translucent);
}
inline void draw_block_dim(int x, int y, uint32_t color) {
	draw_block(x, y, color, dim);
}
//}}}
//{{{ Process operation used by game kernel
void next_round(uint8_t);
void game_over(void);
const int id_start = 1;
const int id_help = 2;
const int id_terminate = 3;

void notify_next_round(uint8_t up) {
	static uint8_t static_up;
	static void (*wait_for_next_round)(void) = [] (void) {
		clear_timer_queue = true;
		next_round(static_up);
	};
	static_up = up;
	if (animation) {
		continuation = wait_for_next_round;
	} else {
		wait_for_next_round();
	}
}

void notify_game_over(void) {
	kill_all_timer();
	clear_timer_queue = true;
	stop_animation();
	SendMessage(window, WM_COMMAND, (WPARAM)BN_CLICKED << 16 | id_terminate, (LPARAM)window);
	game_over();
}
//}}}
//}}}

//{{{ Definition of scene
class scene;
//{{{   Base class of piece
class piece {
public:
	uint32_t color;
	piece(uint32_t c) : color(c) {}
	virtual ~piece(void) {}
	void set_scene(scene *s) { sc = s; }
	virtual int rotate(bool) = 0;
	virtual int shift(bool) = 0;
	virtual int get_distance(void) = 0;
	virtual int get_width(void) = 0;
	virtual int get_height(void) = 0;
	virtual bool is_immobile(void) = 0;
	virtual void draw_piece(int, int, void (*)(int, int, uint32_t)) = 0;
	static piece *random_piece(void);
	static void random_reset(unsigned int);
protected:
	scene *sc = NULL;
private:
	static uint16_t record;
};
//}}}
class scene {
public:
	scene(piece *p) { new_piece(p); level_up(); }
	~scene(void) {
		if (act_pc) delete act_pc;
		if (next_pc) delete next_pc;
	}
	bool stack_test(int, int);
	int get_pos_x(void) { return pos_x; }
	int get_pos_y(void) { return pos_y; }
	bool is_running(void) { return (bool)act_pc; }
	bool is_paused(void) { return paused; }
	bool is_soft_drop(void) { return soft_drop_status; }
	void disable_lock_delay(void) { lock_delay_enabled = false; }
	void shift_piece(bool left) { if (act_pc && !locked) act_pc->shift(left); }
	void rotate_piece(bool cw) { if (act_pc && !locked) act_pc->rotate(cw); }
	//{{{ Get statistical information
	unsigned int get_level(void) { return level; }
	unsigned int get_score(void) { return score; }
	unsigned int get_total_lines(void) { return total_lines; }
	unsigned int get_total_spin(void) { return total_spin; }
	unsigned int get_max_combo(void) { return max_combo; }
	unsigned int get_total_tetris(void) { return total_tetris; }
	unsigned int get_total_spin_clear(void) { return total_spin_clear; }
	unsigned int get_total_b2b(void) { return total_b2b; }
	unsigned int get_total_bravo(void) { return total_bravo; }
	//}}}
	void drop_callback(void) {
		if (!(killed[timer_drop] || paused || locked))
			drop();
	}
	void lock_callback(void) {
		if (!(killed[timer_lock] || paused || locked))
			lock_piece();
	}
	void new_round(piece *new_pc) { new_round_ex(new_pc, false); }
	void move_piece(int, int);
	void draw_piece(void);
	void refresh_piece(void);
	piece *swap_piece(void);
	void drop(void);
	void soft_drop(bool);
	void sonic_drop(bool);
	void lock_piece(void);
	void level_up(void);
	void control(bool);
private:
	piece *act_pc = NULL, *next_pc = NULL;
	uint16_t stack[height] = { 0 };
	static uint16_t piece_mask[4];
	//{{{ State information
	int pos_x, pos_y, distance, top_line = -1;
	int lock_delay, drop_delay;
	unsigned int soft_drop_scale;
	bool locked, paused = false, swapped;
	bool killed[num_timer] = { false };
	bool lock_delay_enabled, drop_delay_enabled;
	bool lock_delay_status, soft_drop_status = false;
	//}}}
	//{{{ Statistical information
	unsigned int level = 0;
	unsigned int score = 0;
	unsigned int total_lines = 0;
	unsigned int total_spin = 0;
	unsigned int max_combo = 0;
	unsigned int total_tetris = 0;
	unsigned int total_spin_clear = 0;
	unsigned int total_b2b = 0;
	unsigned int total_bravo = 0;
	int combo = -1;
	bool b2b_state = false;
	//}}}
	void new_piece(piece *);
	void drop_ex(void);
	void new_round_ex(piece *, bool);
	void set_timer(timer_id timer, int delay) {
		killed[timer] = false;
		::set_timer(timer, delay);
	}
	void kill_timer(timer_id timer) {
		killed[timer] = true;
		::kill_timer(timer);
	}
};
//{{{ Test if the specific position of stack is occupied
bool scene::stack_test(int col, int line) {
	if (line < 0 || line >= height + 4 || col < 0 || col >= width)
		return true;
	if (line >= height)
		return false;
#ifdef ASM
	bool ret;
	asm (
		"bt      %1, %w2     \n\t"
		"setc    %0          \n\t"
	:"=r"(ret):"r"(stack[line]),"r"(col):"cc");
	return ret;
#else
	return stack[line] & 1 << col;
#endif
}
//}}}
//{{{ Put a new piece to the scene
void scene::new_piece(piece *p) {
	if (act_pc)
		delete act_pc;
	if (act_pc = next_pc)
		act_pc->set_scene(this);
	next_pc = p;
}
//}}}
//{{{ Start a new round
void scene::new_round_ex(piece *new_pc, bool swap) {
	if (swapped = swap) {
		piece *pc = act_pc;
		act_pc = next_pc;
		next_pc = pc;
		act_pc->set_scene(this);
		next_pc->set_scene(NULL);
	} else {
		new_piece(new_pc);
	}
	locked = false;
	lock_delay_enabled = true;
	drop_delay_enabled = true;
	lock_delay_status = false;
	soft_drop_scale = 4;
	pos_x = (width - act_pc->get_width()) / 2;
	pos_y = height;
	if ((distance = act_pc->get_distance()) > 0)
		drop_ex();
	else
		notify_game_over();
}
//}}}
//{{{ Move the current piece and reset screen
void scene::move_piece(int dx, int dy) {
	act_pc->draw_piece(pos_x, pos_y, sweep_block);
	if (distance > 0)
		act_pc->draw_piece(pos_x, pos_y - distance, sweep_block);
	pos_x += dx;
	pos_y += dy;
}
//}}}
//{{{ Draw the current piece to screen
void scene::draw_piece(void) {
	if (distance > 0) {
		act_pc->draw_piece(pos_x, pos_y - distance, draw_block_translucent);
		act_pc->draw_piece(pos_x, pos_y, draw_block_normal);
	} else {
		act_pc->draw_piece(pos_x, pos_y,\
			lock_delay_status ? draw_block_normal : draw_block_dim);
	}
}
//}}}
//{{{ Refresh the state accroding to current piece
void scene::refresh_piece(void) {
	int dist = distance;
	if (distance = act_pc->get_distance()) {
		if (dist) {
			draw_piece();
		} else {
			lock_delay_status = false;
			kill_timer(timer_lock);
			if (drop_delay_enabled) {
				draw_piece();
			} else {
				drop_delay_enabled = true;
				drop_ex();
			}
		}
	} else if (!lock_delay_enabled) {
		kill_timer(timer_lock);
		lock_piece();
	} else {
		if (dist) {
			lock_delay_status = true;
			set_timer(timer_lock, lock_delay);
		}
		draw_piece();
	}
}
//}}}
//{{{ Swap the current piece and the next piece
piece *scene::swap_piece(void) {
	if (!act_pc || locked || swapped)
		return NULL;
	kill_timer(timer_drop);
	kill_timer(timer_lock);
	act_pc->draw_piece(pos_x, pos_y, sweep_block);
	if (distance > 0)
		act_pc->draw_piece(pos_x, pos_y - distance, sweep_block);
	new_round_ex(NULL, true);
	return next_pc;
}
//}}}
//{{{ Drop and soft drop
void scene::drop_ex(void) {
	pos_y--;
	if (--distance == 0) {
		if (!lock_delay_enabled) {
			kill_timer(timer_lock);
			lock_piece();
			return;
		}
		lock_delay_status = true;
		set_timer(timer_lock, lock_delay);
	}
	draw_piece();
	set_timer(timer_drop, soft_drop_status ?\
		(int)(drop_delay / sqrt(soft_drop_scale++)) + 1 :\
		drop_delay);
}

void scene::drop(void) {
	if (distance > 0) {
		act_pc->draw_piece(pos_x, pos_y, sweep_block);
		drop_ex();
	} else {
		drop_delay_enabled = false;
	}
}
//}}}
//{{{ Set soft drop delay and status
void scene::soft_drop(bool s) {
	if (!(soft_drop_status = s))
		return;
	soft_drop_scale = 4;
	kill_timer(timer_drop);
	drop();
}
//}}}
//{{{ Sonic drop and hard drop
void scene::sonic_drop(bool hard) {
	if (distance == 0) {
		if (hard) {
			kill_timer(timer_lock);
			lock_piece();
		}
		return;
	}
	act_pc->draw_piece(pos_x, pos_y, sweep_block);
	pos_y -= distance;
	distance = 0;
	if (hard || !lock_delay_enabled) {
		kill_timer(timer_lock);
		lock_piece();
		return;
	}
	lock_delay_status = true;
	set_timer(timer_lock, lock_delay);
	draw_piece();
	set_timer(timer_drop, soft_drop_status ?\
		(int)(drop_delay / sqrt(soft_drop_scale += 1)) + 1 :\
		drop_delay);
}
//}}}
//{{{ Lock the current piece to the stack and clear line
uint16_t scene::piece_mask[4];

void scene::lock_piece(void) {
	int i, y0, y = pos_y;
	int clear[5], num_clear = 0;
	bool immobile, great = false;
	unsigned int point, treasure = 0;
	locked = true;
	lock_delay_status = false;
	draw_piece();
	*(uint64_t *)piece_mask = 0L;
	act_pc->draw_piece(pos_x, 0, [] (int col, int line, uint32_t) {
		piece_mask[line] |= 1 << col;
	});
	immobile = act_pc->is_immobile();
	//{{{ Modify stack
	for (i = 0; i < 4; y++, i++) {
		if (!piece_mask[i])
			continue;
		if ((y > top_line) && ((top_line = y) >= height)) {
			notify_game_over();
			return;
		}
		if (immobile) {
			uint16_t t = piece_mask[i];
			t -= t >> 1 & 0x5555;
			t = (t & 0x3333) + (t >> 2 & 0x3333);
			t += (t += t >> 4) >> 8;
			stack[y] += (t & 0xF) << width;
		}
		if (!(~(stack[y] |= piece_mask[i]) & ((1 << width) - 1))) {
			clear[num_clear++] = y;
			treasure += stack[y] >> width;
		}
	}
	if (immobile)
		total_spin++;
	//}}}
	//{{{ Line clear
	if (num_clear == 0) {
		combo = -1;
		notify_next_round(0);
		return;
	}
	clear[num_clear] = height;
	y = y0 = clear[0];
	i = 1;
	while (y < top_line) {
		if (++y < clear[i])
			stack[y0++] = stack[y];
		else
			i++;
	}
	while (y >= y0)
		stack[y--] = 0;
	clear_line(clear, top_line);
	top_line = y;
	total_lines += num_clear;
	//}}}
	//{{{ Score system
	if (++combo > max_combo)
		max_combo = combo;
	switch (num_clear) {
	case 1:
		if (immobile) {
			great = true;
			total_spin_clear++;
			point = 80;
		} else {
			point = 10;
		}
		break;
	case 2:
		if (immobile) {
			great = true;
			total_spin_clear++;
			point = 120;
		} else {
			point = 30;
		}
		break;
	case 3:
		point = 50;
		break;
	case 4:
		great = true;
		total_tetris++;
		point = 80;
		break;
	}
	if (top_line < 0) {
		great = true;
		total_bravo++;
		point += 200;
	}
	point += treasure * 4;
	if (!great) {
		b2b_state = false;
	} else if (b2b_state) {
		total_b2b++;
		point = point / 2 * 3;
	} else {
		b2b_state = true;
	}
	point += combo * 5;
	score += point * level;
	//}}}
	notify_next_round(total_lines >= level * 20 ? (level_up(), 2) : 1);
	return;
}
//}}}
//{{{ Level speed control
void scene::level_up(void) {
	if (++level == 1) {
		drop_delay = lock_delay = 1000;
	} else if (level <= 20) {
		drop_delay = 1000 / level;
		lock_delay = 500;
	} else {
		drop_delay = 50;
		lock_delay = (int)(500 / sqrt(level - 19)) + 1;
	}
}
//}}}
//{{{ Game process control
void scene::control(bool pause) {
	if (!paused) {
		if (pause) {
			paused = true;
			stop_timer(timer_drop);
			stop_timer(timer_lock);
		}
	} else {
		if (!pause) {
			paused = false;
			resume_timer(timer_drop);
			resume_timer(timer_lock);
		} else {
			notify_game_over();
		}
	}
}
//}}}
//}}}
//{{{ Definition of piece
//{{{   Dervied class of pieces that contained in 3x3 box,
//      including T, L, J, S, Z and O
class piece_3 : public piece {
public:
	piece_3(uint8_t s, uint32_t c) : shape_code(expand(s)), piece(c) {}
	virtual int rotate(bool) = 0;
	virtual int shift(bool);
	virtual int get_distance(void);
	virtual int get_width(void);
	virtual int get_height(void);
	virtual bool is_immobile(void);
	virtual void draw_piece(int, int, void (*)(int, int, uint32_t));
protected:
	uint16_t shape_code;
	static uint16_t expand(uint8_t);
	int rotate_gen(uint16_t, bool);
};
//{{{   Expand shape data
uint16_t piece_3::expand(uint8_t src) {
	uint16_t ret;
#ifdef ASM
	uint16_t tmp;
	asm (
		"movzbw  %0, %1      \n\t"
		"test    %b0, %b0    \n\t"
		"jp      .L%=.0      \n\t"
		"stc                 \n.L%=.0:\t"
		"rcl     %0, 1       \n\t"
		"mov     %2, %0      \n\t"
		"and     %2, 7       \n\t"
		"bsf     %w1, %2     \n\t"
		"jz      .L%=.1      \n\t"
		"xor     %1, 3       \n\t"
		"shl     %w1, 9      \n\t"
		"or      %0, %w1     \n.L%=.1:\t"
		"mov     %2, %0      \n\t"
		"shr     %2, 3       \n\t"
		"and     %2, 7       \n\t"
		"bsf     %w1, %2     \n\t"
		"jz      .L%=.2      \n\t"
		"xor     %1, 3       \n\t"
		"shl     %w1, 11     \n\t"
		"or      %0, %w1     \n.L%=.2:\t"
		"mov     %2, %0      \n\t"
		"shr     %2, 6       \n\t"
		"and     %2, 7       \n\t"
		"bsf     %w1, %2     \n\t"
		"jz      .L%=.3      \n\t"
		"xor     %1, 3       \n\t"
		"shl     %w1, 13     \n\t"
		"or      %0, %w1     \n.L%=.3:\t"
	:"=&r"(ret):"r"(src),"r"(tmp):"cc");
#else
	ret = src;
	src ^= src >> 4;
	src ^= src >> 2;
	src ^= src >> 1;
	ret <<= 1;
	ret |= src & 1;
	src = ret & 7;
	src = src & 1 ? 3 : src & 2 ? 2 : src & 4 ? 1 : 0;
	ret |= (uint16_t)src << 9;
	src = ret >> 3 & 7;
	src = src & 1 ? 3 : src & 2 ? 2 : src & 4 ? 1 : 0;
	ret |= (uint16_t)src << 11;
	src = ret >> 6 & 7;
	src = src & 1 ? 3 : src & 2 ? 2 : src & 4 ? 1 : 0;
	ret |= (uint16_t)src << 13;
#endif
	return ret;
}
//}}}
//{{{   Rotate the piece to the new shape and set the position in scene,
//      non-negative return value means a successful rotation.
int piece_3::rotate_gen(uint16_t new_shape_code, bool center_column) {
	const int offset[] = { 0, 1, -1 };
	int x, y;
	if (!sc) {
		shape_code = new_shape_code;
		return 0;
	}
	x = sc->get_pos_x();
	y = sc->get_pos_y();
	for (int k = 0; k < sizeof(offset) / sizeof(offset[0]); k++) {
		int bit, xp = x + offset[k];
		for (int j = 2; bit = 6 + j, j >= 0; j--)
		for (int i = 0; i < 3; i++, bit -= 3)
		if ((new_shape_code & (1 << bit)) &&\
			sc->stack_test(xp + i, y + j))
			if (center_column && k == 0 && i == 1)
				return -1;
			else
				goto __continue;
		sc->move_piece(offset[k], 0);
		shape_code = new_shape_code;
		return k; __continue:;
	}
	return -1;
}
//}}}
//{{{   Shift the piece to the new position in scene,
//      non-negative return value means a successful shift.
int piece_3::shift(bool left) {
	uint16_t code = shape_code;
	int x, y, dx = left ? -1 : 1;
	x = sc->get_pos_x() + dx;
	y = sc->get_pos_y();
	for (int i = 2; i >= 0; i--)
	for (int j = 0; j < 3; j++, code >>= 1)
	if ((code & 1) && sc->stack_test(x + i, y + j))
		return -1;
	sc->move_piece(dx, 0);
	sc->refresh_piece();
	return 0;
}
//}}}
//{{{   Get the distance from piece to the stack
int piece_3::get_distance(void) {
	int shift, x, y, dist;
	x = sc->get_pos_x();
	y = sc->get_pos_y() + 2;
	for (dist = 0; dist < height; y--, dist++) {
		if ((shift = shape_code >> 9 & 3) &&\
			sc->stack_test(x + 2, y - shift))
			break;
		if ((shift = shape_code >> 11 & 3) &&\
			sc->stack_test(x + 1, y - shift))
			break;
		if ((shift = shape_code >> 13 & 3) &&\
			sc->stack_test(x, y - shift))
			break;
	}
	return dist;
}
//}}}
//{{{   Get the centering box of the piece
int piece_3::get_width(void) {
	return (shape_code & 0700 ? 0 : shape_code & 0070 ? 1 : 2) +\
		(shape_code & 0007 ? 2 : shape_code & 0070 ? 1 : 0) + 1;
}
int piece_3::get_height(void) {
	return (shape_code & 0111 ? 0 : shape_code & 0222 ? 1 : 2) +\
		(shape_code & 0444 ? 2 : shape_code & 0222 ? 1 : 0) + 1;
}
//}}}
//{{{   Decide if the piece is immobile
bool piece_3::is_immobile(void) {
	int x, y, dx = 1, dy = 0;
	x = sc->get_pos_x();
	y = sc->get_pos_y();
	for (int k = 3; k > 0; k--) {
		uint16_t code = shape_code;
		for (int i = 2; i >= 0; i--)
		for (int j = 0; j < 3; j++, code >>= 1)
		if ((code & 1) && sc->stack_test(x + dx + i, y + dy + j))
			goto __continue;
		return false;
		__continue: int t = dy; dy = dx; dx = -t;
	}
	return true;
}
//}}}
//{{{   Draw the piece using the giving drawer
void piece_3::draw_piece(int x, int y, void (*drawer)(int, int, uint32_t)) {
	uint16_t code = shape_code;
	for (int i = 2; i >= 0; i--)
	for (int j = 0; j < 3; j++, code >>= 1)
	if (code & 1)
		drawer(x + i, y + j, color);
}
//}}}

//{{{     Dervied class of piece T
class piece_T : public piece_3 {
public:
	piece_T(void) : piece_3(shape[0], cyan) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
	uint8_t direction = 0;
	bool kicked = false;
	int kick_floor(uint16_t);
};
const uint8_t piece_T::shape[] = { 0x4D, 0x5C, 0x2C, 0x1D };
int piece_T::kick_floor(uint16_t new_shape_code) {
	int x, y;
	x = sc->get_pos_x();
	y = sc->get_pos_y() + 1;
	if (sc->stack_test(x, y) || sc->stack_test(x + 1, y) ||\
		sc->stack_test(x + 2, y) || sc->stack_test(x + 1, y + 1))
		return -1;
	sc->move_piece(0, 1);
	shape_code = new_shape_code;
	kicked = true;
	return 3;
}
int piece_T::rotate(bool cw) {
	int res;
	uint16_t code;
	bool k = kicked;
	uint8_t d = (direction + (cw ? 1 : -1)) % (sizeof(shape) / sizeof(shape[0]));
	if ((res = rotate_gen(code = expand(shape[d]), true)) >= 0 ||\
		d == 2 && (res = kick_floor(code)) >= 0) {
		direction = d;
		if (sc) {
			if (k)
				sc->disable_lock_delay();
			sc->refresh_piece();
		}
	}
	return res;
}
//}}}
//{{{     Dervied class of piece L
class piece_L : public piece_3 {
public:
	piece_L(void) : piece_3(shape[0], orange) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
	uint8_t direction = 0;
};
const uint8_t piece_L::shape[] = { 0x69, 0x9C, 0x25, 0x1C };
int piece_L::rotate(bool cw) {
	int res;
	uint8_t d = (direction + (cw ? 1 : -1)) % (sizeof(shape) / sizeof(shape[0]));
	if ((res = rotate_gen(expand(shape[d]), true)) >= 0) {
		direction = d;
		if (sc)
			sc->refresh_piece();
	}
	return res;
}
//}}}
//{{{     Dervied class of piece J
class piece_J : public piece_3 {
public:
	piece_J(void) : piece_3(shape[0], blue) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
	uint8_t direction = 0;
};
const uint8_t piece_J::shape[] = { 0x49, 0x3C, 0x64, 0x1E };
int piece_J::rotate(bool cw) {
	int res;
	uint8_t d = (direction + (cw ? 1 : -1)) % (sizeof(shape) / sizeof(shape[0]));
	if ((res = rotate_gen(expand(shape[d]), true)) >= 0) {
		direction = d;
		if (sc)
			sc->refresh_piece();
	}
	return res;
}
//}}}
//{{{     Dervied class of piece S
class piece_S : public piece_3 {
public:
	piece_S(void) : piece_3(shape[0], purple) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
	uint8_t direction = 0;
};
const uint8_t piece_S::shape[] = { 0x2D, 0xCC };
int piece_S::rotate(bool cw) {
	int res;
	uint8_t d = (direction + (cw ? 1 : -1)) % (sizeof(shape) / sizeof(shape[0]));
	if ((res = rotate_gen(expand(shape[d]), false)) >= 0) {
		direction = d;
		if (sc)
			sc->refresh_piece();
	}
	return res;
}
//}}}
//{{{     Dervied class of piece Z
class piece_Z : public piece_3 {
public:
	piece_Z(void) : piece_3(shape[0], green) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
	uint8_t direction = 0;
};
const uint8_t piece_Z::shape[] = { 0x4C, 0x0F };
int piece_Z::rotate(bool cw) {
	int res;
	uint8_t d = (direction + (cw ? 1 : -1)) % (sizeof(shape) / sizeof(shape[0]));
	if ((res = rotate_gen(expand(shape[d]), false)) >= 0) {
		direction = d;
		if (sc)
			sc->refresh_piece();
	}
	return res;
}
//}}}
//{{{     Dervied class of piece O
class piece_O : public piece_3 {
public:
	piece_O(void) : piece_3(shape[0], yellow) {}
	virtual int rotate(bool);
private:
	static const uint8_t shape[];
};
const uint8_t piece_O::shape[] = { 0x0D };
int piece_O::rotate(bool) { return 0; }
//}}}
//}}}
//{{{   Dervied class of piece I
class piece_I : public piece {
public:
	piece_I(void) : piece(red) {}
	virtual int rotate(bool);
	virtual int shift(bool);
	virtual int get_distance(void);
	virtual int get_width(void);
	virtual int get_height(void);
	virtual bool is_immobile(void);
	virtual void draw_piece(int, int, void (*)(int, int, uint32_t));
private:
	bool lay_down = true;
	bool kicked = false;
};
//{{{   Rotate
int piece_I::rotate(bool) {
	int x, y, new_x, new_y, res;
	bool k = kicked;
	if (!sc) {
		lay_down = !lay_down;
		return 0;
	}
	x = sc->get_pos_x();
	y = sc->get_pos_y();
	if (lay_down) {
		new_x = x + 2;
		new_y = y - 2;
		if (sc->stack_test(new_x, new_y + 3))
			return -1;
		else if (sc->stack_test(new_x, new_y + 1))
			if (sc->stack_test(new_x, new_y + 4) ||\
				sc->stack_test(new_x, new_y + 5))
				return -1;
			else
				res = 2;
		else if (sc->stack_test(new_x, new_y))
			if (sc->stack_test(x, y - 1) ||\
				sc->stack_test(x + 1, y - 1) ||\
				sc->stack_test(x + 3, y - 1))
				if (sc->stack_test(new_x, new_y + 4))
					return -1;
				else
					res = 1;
			else
				return -1;
		else
			res = 0;
	} else {
		new_x = x - 2;
		new_y = y + 2;
		if (!(sc->stack_test(new_x, new_y) ||\
			sc->stack_test(new_x + 1, new_y) ||\
			sc->stack_test(new_x + 3, new_y)))
			res = 0;
		else if (!(sc->stack_test(x - 1, y) ||\
			sc->stack_test(x - 1, y + 1) ||\
			sc->stack_test(x - 1, y + 2) ||\
			sc->stack_test(x - 1, y + 3) ||\
			sc->stack_test(x + 1, y) ||\
			sc->stack_test(x + 1, y + 1) ||\
			sc->stack_test(x + 1, y + 2) ||\
			sc->stack_test(x + 1, y + 3)))
			return -1;
		else if (!(sc->stack_test(new_x + 1, new_y) ||\
			sc->stack_test(new_x + 3, new_y) ||\
			sc->stack_test(new_x + 4, new_y)))
			res = 1;
		else if (!(sc->stack_test(new_x - 1, new_y) ||\
			sc->stack_test(new_x, new_y) ||\
			sc->stack_test(new_x + 1, new_y)))
			res = -1;
		else if (!(sc->stack_test(new_x + 3, new_y) ||\
			sc->stack_test(new_x + 4, new_y) ||\
			sc->stack_test(new_x + 5, new_y)))
			res = 2;
		else
			return -1;
	}
	if (lay_down)
		kicked = res > 0, sc->move_piece(2, -2 + res);
	else
		sc->move_piece(-2 + res, 2);
	lay_down = !lay_down;
	if (k)
		sc->disable_lock_delay();
	sc->refresh_piece();
	return res;
}
//}}}
//{{{   Shift
int piece_I::shift(bool left) {
	int x, y;
	x = sc->get_pos_x();
	y = sc->get_pos_y();
	if (lay_down) {
		if (left && sc->stack_test(x - 1, y) ||\
			!left && sc->stack_test(x + 4, y))
			return -1;
	} else {
		if (left && (\
				sc->stack_test(x - 1, y) ||\
				sc->stack_test(x - 1, y + 1) ||\
				sc->stack_test(x - 1, y + 2) ||\
				sc->stack_test(x - 1, y + 3)) ||\
			!left && (\
				sc->stack_test(x + 1, y) ||\
				sc->stack_test(x + 1, y + 1) ||\
				sc->stack_test(x + 1, y + 2) ||\
				sc->stack_test(x + 1, y + 3)))
			return -1;
	}
	sc->move_piece(left ? -1 : 1, 0);
	sc->refresh_piece();
	return 0;
}
//}}}
//{{{   Get the distance
int piece_I::get_distance(void) {
	int x, y, dist;
	x = sc->get_pos_x();
	y = sc->get_pos_y() - 1;
	if (lay_down)
		for (dist = 0; !(\
			sc->stack_test(x, y) ||\
			sc->stack_test(x + 1, y) ||\
			sc->stack_test(x + 2, y) ||\
			sc->stack_test(x + 3, y)); y--, dist++);
	else
		for (dist = 0; !sc->stack_test(x, y); y--, dist++);
	return dist;
}
//}}}
//{{{   Get the centering box
int piece_I::get_width(void) {
	return lay_down ? 4 : 1;
}
int piece_I::get_height(void) {
	return lay_down ? 1 : 4;
}
//}}}
//{{{   Decide if the piece is immobile
bool piece_I::is_immobile(void) {
	int x, y;
	x = sc->get_pos_x();
	y = sc->get_pos_y();
	return lay_down &&\
		sc->stack_test(x - 1, y) && sc->stack_test(x + 4, y) && (\
			sc->stack_test(x, y + 1) ||\
			sc->stack_test(x + 1, y + 1) ||\
			sc->stack_test(x + 2, y + 1) ||\
			sc->stack_test(x + 3, y + 1)) || !lay_down &&\
		sc->stack_test(x, y + 4) && (\
			sc->stack_test(x - 1, y) ||\
			sc->stack_test(x - 1, y + 1) ||\
			sc->stack_test(x - 1, y + 2) ||\
			sc->stack_test(x - 1, y + 3)) && (\
			sc->stack_test(x + 1, y) ||\
			sc->stack_test(x + 1, y + 1) ||\
			sc->stack_test(x + 1, y + 2) ||\
			sc->stack_test(x + 1, y + 3));
}
//}}}
//{{{   Draw the piece
void piece_I::draw_piece(int x, int y, void (*drawer)(int, int, uint32_t)) {
	drawer(x, y, color);
	if (lay_down) {
		drawer(x + 1, y, color);
		drawer(x + 2, y, color);
		drawer(x + 3, y, color);
	} else {
		drawer(x, y + 1, color);
		drawer(x, y + 2, color);
		drawer(x, y + 3, color);
	}
}
//}}}
//}}}
//{{{ Piece randomizer
uint16_t piece::record;

piece *piece::random_piece(void) {
	uint16_t index, rec;
	piece *ret;
	for (int i = 6; i > 0; i--) {
		index = rand() % 7;
		if ((index != ((rec = record) & 0xf)) &&\
			(index != ((rec >>= 4) & 0xf)) &&\
			(index != ((rec >>= 4) & 0xf)) &&\
			(index != (rec >>= 4)))
			break;
	}
	record = record << 4 | index;
	switch (index) {
	case 0: return new piece_T;
	case 1: return new piece_O;
	case 2: return new piece_I;
	case 3: return new piece_S;
	case 4: return new piece_Z;
	case 5: return new piece_L;
	case 6: return new piece_J;
	}
	return NULL;
}

void piece::random_reset(unsigned int seed) {
	record = 0x4343;
	srand(seed);
}
//}}}
//}}}

//{{{ UI control -- Part II
scene *game_scene;
//{{{ Draw next piece
void draw_next(piece *pc) {
	static int offset_x, offset_y;
	offset_x = (next_board->get_width() - pc->get_width() * unit) / 2;
	offset_y = (next_board->get_height() - pc->get_height() * unit) / 2;
	next_board->flush_board();
	pc->draw_piece(0, 0, [] (int x, int y, uint32_t color) {
		next_board->show_block(offset_x, offset_y, x, y, color, normal);
	});
}
//}}}
//{{{ Next round
void next_round(void) {
	piece *pc = piece::random_piece();
	draw_next(pc);
	game_scene->new_round(pc);
}

void next_round(uint8_t up) {
	if (up) {
		score_board->show_digit(game_scene->get_score());
		lines_board->show_digit(game_scene->get_total_lines());
		if (--up)
			level_board->show_digit(game_scene->get_level());
	}
	next_round();
}
//}}}
//{{{ Hold piece
void hold_piece(void) {
	piece *pc = game_scene->swap_piece();
	if (pc) {
		clear_timer_queue = true;
		draw_next(pc);
	}
}
//}}}
//{{{ Start game
void start_game(void) {
	static gc *dummy = new gc(NULL, [] (void *) {
		if (game_scene) {
			delete game_scene;
			game_scene = NULL;
		}
	});
	kill_timer(timer_animation);
	clear_timer_queue = true;
	stop_animation();
	piece::random_reset(time(NULL));
	piece *pc = piece::random_piece();
	game_scene = new scene(pc);
	draw_next(pc);
	level_board->show_digit(game_scene->get_level());
	score_board->show_digit(game_scene->get_score());
	lines_board->show_digit(game_scene->get_total_lines());
	continuation = next_round;
	ready_go();
}
//}}}
//{{{ Game over
void game_over(void) {
	const char *str_title = "GAME OVER";
	const char *str_stat[] = {
		"Score            %9u",
		"Total Line       %9u",
		"Total Spin       %9u",
		"Max Combo        %9u",
		"Total Tetris     %9u",
		"Total Spin Clear %9u",
		"Total B2B        %9u",
		"Total Bravo      %9u"
	};
	const int num_stat = sizeof(str_stat) / sizeof(str_stat[0]);
	const int size = 3;
	const int len_line = strlen(str_stat[0]) - 3 + 9;
	const unsigned int max_digit = 1000000000;
	static int wd = main_board->get_width();
	static int ht = main_board->get_height();
	static int ht_title = ht / 5 + 16 * size / 2;
	static int x_title = (wd - 8 * size * strlen(str_title)) / 2;
	static int y_title = ht_title - 16 * size;
	static int ht_stat = ht * 2 / 5 + 16 * 8 / 2;
	static int x_stat = (wd - 8 * len_line) / 2;
	static unsigned int stat[num_stat];
	static bool started;
	static HBITMAP bmp_title, bmp_stat;
	static void (*ani_game_over)(void) = [] (void) {
		static int frame;
		if (!animation) {
			frame = started ? -height : 0;
			animation = ani_game_over;
		}
		if (frame < 0) {
			main_board->move_line(frame + height, -1);
			set_timer(timer_animation, 50);
		} else if (frame < 20) {
			if (frame == 0) {
				main_board->flush_board();
				next_board->flush_board();
			}
			main_board->draw_bitmap(bmp_title,\
				0, ht_title * (frame - 19) / 20, wd, ht_title);
			if (started)
				main_board->draw_bitmap(bmp_stat,\
					0, ht - ht_stat * (frame + 1) / 20, wd, ht_stat);
			set_timer(timer_animation, 50);
		} else {
			default_animation_end();
		}
		frame++;
	};
	if (!bmp_title) {
		new gc((collector)DeleteObject,
		bmp_title = new_bitmap(wd, ht_title));
		HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp_title);
		fill_bg(hdc_sub, wd, ht_title);
		put_string(hdc_sub, false, false, size,\
			x_title, y_title, str_title, -1, 0, white);
		SelectObject(hdc_sub, old_bmp);
	}
	if (!bmp_stat) {
		new gc((collector)DeleteObject,
		bmp_stat = new_bitmap(wd, ht_stat));
	}
	started = game_scene->is_running();
	stat[0] = game_scene->get_score() % max_digit;
	stat[1] = game_scene->get_total_lines() % max_digit;
	stat[2] = game_scene->get_total_spin() % max_digit;
	stat[3] = game_scene->get_max_combo() % max_digit;
	stat[4] = game_scene->get_total_tetris() % max_digit;
	stat[5] = game_scene->get_total_spin_clear() % max_digit;
	stat[6] = game_scene->get_total_b2b() % max_digit;
	stat[7] = game_scene->get_total_bravo() % max_digit;
	if (started) {
		HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_sub, bmp_stat);
		char line[len_line + 1];
		fill_bg(hdc_sub, wd, ht_stat);
		for (int i = 0; i < num_stat; i++) {
			sprintf(line, str_stat[i], stat[i]);
			put_string(hdc_sub, false, false, 1,\
				x_stat, i * 16, line, -1, 0, white);
		}
		SelectObject(hdc_sub, old_bmp);
	}
	delete game_scene;
	game_scene = NULL;
	continuation = show_language;
	ani_game_over();
}
//}}}
//}}}

//{{{ Message Loop
LRESULT CALLBACK window_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp) {
	HDC hdc;
	PAINTSTRUCT ps;
	static HWND button_start, button_help, button_terminate;
	static HICON ico_start, ico_pause, ico_term, ico_term_gray;
	static bool state_help = false;
	const int d = 15;
	const int x = unit * (width + 2);
	const int y = unit * (height + 1);
	const int size_big = (10 * unit - d) / 3;
	const int size_small = (5 * unit - 2 * d) / 3;
	switch (msg) {
	case WM_CREATE: //{{{
		hdc = GetDC(win);
		new gc((collector)DeleteDC,
		hdc_main = CreateCompatibleDC(hdc));
		new gc((collector)DeleteDC,
		hdc_sub = CreateCompatibleDC(hdc));
		DeleteObject(SelectObject(hdc_main,\
			CreateCompatibleBitmap(hdc, client_width, client_height)));
		ReleaseDC(win, hdc);
		button_start = CreateWindow("button", NULL,\
			WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_ICON,\
			x, y - size_big, size_big, size_big,\
			win, (HMENU)id_start, instance, NULL);
		new gc((collector)DestroyIcon,
		ico_start = LoadIcon(instance, MAKEINTRESOURCE(102)));
		new gc((collector)DestroyIcon,
		ico_pause = LoadIcon(instance, MAKEINTRESOURCE(103)));
		SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_start);
		button_help = CreateWindow("button", NULL,\
			WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_PUSHLIKE | BS_ICON,\
			client_width - unit - size_small, y - size_big,\
			size_small, size_small,\
			win, (HMENU)id_help, instance, NULL);
		new gc((collector)DestroyIcon,
		ico_term = LoadIcon(instance, MAKEINTRESOURCE(104)));
		SendMessage(button_help, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_term);
		button_terminate = CreateWindow("button", NULL,\
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED | BS_ICON,\
			client_width - unit - size_small, y - size_small,\
			size_small, size_small,\
			win, (HMENU)id_terminate, instance, NULL);
		new gc((collector)DestroyIcon,
		ico_term = LoadIcon(instance, MAKEINTRESOURCE(105)));
		new gc((collector)DestroyIcon,
		ico_term_gray = LoadIcon(instance, MAKEINTRESOURCE(106)));
		SendMessage(button_terminate, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_term_gray);
		return 0; //}}}
	case WM_SHOWWINDOW: //{{{
		init_board();
		welcome();
		return 0; //}}}
	case WM_PAINT: //{{{
		hdc = BeginPaint(win, &ps);
		BitBlt(hdc, 0, 0, client_width, client_height, hdc_main, 0, 0, SRCCOPY);
		EndPaint(win, &ps);
		return 0; //}}}
	case WM_TIMER: //{{{
		switch ((timer_id)(wp - 1)) {
		case timer_drop:
			kill_timer(timer_drop);
			game_scene->drop_callback();
			break;
		case timer_lock:
			kill_timer(timer_lock);
			game_scene->lock_callback();
			break;
		case timer_animation:
			kill_timer(timer_animation);
			if (animation)
				animation();
			break;
		}
		return 0; //}}}
	case WM_COMMAND: //{{{
		if (lp && wp >> 16 == BN_CLICKED)
		switch (wp & 0xFFFF) {
		case id_start: //{{{
			if (state_help) {
				state_help = false;
				Button_SetCheck(button_help, BST_UNCHECKED);
			}
			if (!game_scene) {
				SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_pause);
				SendMessage(button_terminate, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_term);
				Button_Enable(button_terminate, true);
				start_game();
			} else if (game_scene->is_paused()) {
				SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_pause);
				main_board->restore_bitmap();
				next_board->restore_bitmap();
				resume_timer(timer_animation);
				game_scene->control(false);
			} else {
				SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_start);
				game_scene->control(true);
				stop_timer(timer_animation);
				clear_timer_queue = true;
				main_board->save_bitmap();
				next_board->save_bitmap();
				next_board->flush_board();
				draw_pause();
			}
			break; //}}}
		case id_help: //{{{
			if (state_help) {
				state_help = false;
				Button_SetCheck(button_help, BST_UNCHECKED);
				if (!game_scene) {
					if (continuation) {
						main_board->restore_bitmap();
						next_board->restore_bitmap();
						resume_timer(timer_animation);
					} else {
						main_board->restore_bitmap();
					}
				} else if (game_scene->is_paused()) {
					draw_pause();
				}
			} else {
				state_help = true;
				Button_SetCheck(button_help, BST_CHECKED);
				if (!game_scene) {
					if (continuation) {
						stop_timer(timer_animation);
						clear_timer_queue = true;
						main_board->save_bitmap();
						next_board->save_bitmap();
						next_board->flush_board();
						draw_help();
					} else {
						main_board->save_bitmap();
						draw_help();
					}
				} else if (game_scene->is_paused()) {
					draw_help();
				} else {
					SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_start);
					game_scene->control(true);
					stop_timer(timer_animation);
					clear_timer_queue = true;
					main_board->save_bitmap();
					next_board->save_bitmap();
					next_board->flush_board();
					draw_help();
				}
			}
			break; //}}}
		case id_terminate: //{{{
			if (!game_scene)
				break;
			if ((HWND)lp == win) {
				SendMessage(button_start, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico_start);
				Button_Enable(button_terminate, false);
				break;
			}
			if (state_help) {
				state_help = false;
				Button_SetCheck(button_help, BST_UNCHECKED);
			}
			if (!game_scene->is_paused()) {
				game_scene->control(true);
			} else {
				main_board->restore_bitmap();
				next_board->restore_bitmap();
			}
			game_scene->control(true);
			break; //}}}
		}
		SetFocus(win);
		return 0; //}}}
	case WM_KEYUP: //{{{
		if (wp == VK_DOWN && game_scene && game_scene->is_running())
			game_scene->soft_drop(false);
		return 0; //}}}
	case WM_KEYDOWN: //{{{
		switch (wp) {
		case VK_LEFT: //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->shift_piece(true);
			break; //}}}
		case VK_RIGHT: //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->shift_piece(false);
			break; //}}}
		case VK_UP: //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->rotate_piece(true);
			break; //}}}
		case 'Z': //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->rotate_piece(false);
			break; //}}}
		case VK_DOWN: //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->soft_drop(true);
			break; //}}}
		case VK_SPACE: //{{{
			if (game_scene && game_scene->is_running() &&\
				!game_scene->is_paused())
				game_scene->sonic_drop(!game_scene->is_soft_drop());
			break; //}}}
		case VK_RETURN: //{{{
			if (game_scene) {
				if (game_scene->is_soft_drop() &&\
					game_scene->is_running() &&\
					!game_scene->is_paused())
					hold_piece();
				else
					SendMessage(win, WM_COMMAND, (WPARAM)BN_CLICKED << 16 | id_start, (LPARAM)win);
			}
			break; //}}}
		}
		return 0; //}}}
	case WM_CLOSE: case WM_DESTROY: //{{{
		PostQuitMessage(0);
		return 0; //}}}
	}
	return DefWindowProc(win, msg, wp, lp);
}
//}}}
//{{{ WinMain()
int APIENTRY WinMain(HINSTANCE inst, HINSTANCE, LPTSTR, int) {
	const char *app_name = "Tetris";
	const unsigned int style =\
		(WS_OVERLAPPEDWINDOW | WS_VISIBLE) &\
		~(WS_MAXIMIZEBOX | WS_SIZEBOX);
	MSG message;
	WNDCLASSEX window_class;
	RECT rect;
	instance = inst;
	//{{{ Register window class
	window_class.cbSize        = sizeof(WNDCLASSEX);
	window_class.style         = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc   = window_proc;
	window_class.cbClsExtra    = 0;
	window_class.cbWndExtra    = 0;
	window_class.hInstance     = instance;
	new gc((collector)DestroyIcon,
	window_class.hIcon         = LoadIcon(instance, MAKEINTRESOURCE(100)));
	window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
	new gc((collector)DeleteObject,
	window_class.hbrBackground = CreateSolidBrush(gray));
	window_class.lpszMenuName  = NULL;
	window_class.lpszClassName = app_name;
	new gc((collector)DestroyIcon,
	window_class.hIconSm       = LoadIcon(instance, MAKEINTRESOURCE(101)));
	if (!RegisterClassEx(&window_class)) {
		MessageBox(NULL, "RegisterClassEx() Failed!", "Fatal", MB_ICONERROR);
		gc::garbage_collect();
		return -1;
	}
	//}}}
	//{{{ Create window at center
	rect.left = rect.right = 0;
	rect.bottom = client_height;
	rect.right = client_width;
	AdjustWindowRect(&rect, style, false);
	rect.right -= rect.left;
	rect.bottom -= rect.top;
	if (!(window = CreateWindow(app_name, app_name, style,\
		(GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2,\
		(GetSystemMetrics(SM_CYSCREEN) - rect.bottom) / 2,\
		rect.right, rect.bottom,\
		NULL, NULL, instance, NULL))) {
		MessageBox(NULL, "CreateWindow() Failed!", "Fatal", MB_ICONERROR);
		gc::garbage_collect();
		return -1;
	}
	ShowWindow(window, SW_SHOW);
	UpdateWindow(window);
	//}}}
	//{{{ Message dispatch
	while (GetMessage(&message, NULL, 0, 0)) {
		if (clear_timer_queue) {
			clear_timer_queue = false;
			PeekMessage(&message, window, WM_TIMER, WM_TIMER, PM_REMOVE);
		}
		DispatchMessage(&message);
	}
	//}}}
	gc::garbage_collect();
	return (int)message.wParam;
}
//}}}
