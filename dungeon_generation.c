#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <endian.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <ncurses.h>


#include "heap.h"

# define NPC_INTELLIGENT 	0x00000001
# define NPC_TELEPATHIC	 	0x00000002
# define NPC_TUNNEL		 	0x00000004
# define NPC_ERRATIC	 	0x00000008
//# define NPC_BIT05 			0x00000010
//# define NPC_BIT06 			0x00000020
//# define NPC_BIT07 			0x00000040
//# define NPC_BIT08 			0x00000080
//# define NPC_BIT09 			0x00000100
//# define NPC_BIT10 			0x00000200
//# define NPC_BIT11 			0x00000400
//# define NPC_BIT12 			0x00000800
//# define NPC_BIT13 			0x00001000
//# define NPC_BIT14 			0x00002000
//# define NPC_BIT15 			0x00004000
//# define NPC_BIT16 			0x00008000
//# define NPC_BIT17 			0x00010000
//# define NPC_BIT18 			0x00020000
//# define NPC_BIT19 			0x00040000
//# define NPC_BIT20 			0x00080000
//# define NPC_BIT21 			0x00100000
//# define NPC_BIT22 			0x00200000
//# define NPC_BIT23 			0x00400000
//# define NPC_BIT24 			0x00800000
//# define NPC_BIT25 			0x01000000
//# define NPC_BIT26 			0x02000000
//# define NPC_BIT27 			0x04000000
//# define NPC_BIT28 			0x08000000
//# define NPC_BIT29 			0x10000000
//# define NPC_BIT30 			0x20000000
//# define NPC_BIT31 			0x40000000
//# define NPC_BIT32 			0x80000000

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define NUMROWS 21
#define NUMCOLS 80
#define MIN_WIDTH 3
#define MIN_HEIGHT 2
#define NUM_OF_ROOMS 15
#define MAX_NUM_MONSTERS 1000

#define FALSE 0
#define TRUE 1
#define ROOM '.'
#define CORRIDOR '#'
#define ROCK ' '
#define PC '@'
#define MONSTER 'M'

typedef enum dim {
  dim_x,			//automatically assigned 0
  dim_y,			//automatically assigned 1
  num_dims			//automatically assigned 2
} dim_t;
typedef int16_t pair_t[num_dims];

typedef enum __attribute__ ((__packed__)) terrain_type {
  ter_debug,
  ter_wall,
  ter_wall_immutable,
  ter_floor,
  ter_floor_room,
  ter_floor_hall,
} terrain_type_t;

typedef struct room{
	pair_t position;
	pair_t size;
	int x_pos;
	int y_pos;
	int width;
	int height;
}room_t;

typedef struct dijkstra {
  heap_node_t *hn;
  uint8_t pos[2];
  int32_t cost;
} dijkstra_t;

typedef struct turn {
  heap_node_t *hn;
  uint8_t pos[2];
  uint8_t booty[2];
  uint16_t ability;
  int32_t speed;
  int32_t current_turn;
  int32_t next_turn;
  int32_t sequence_number;
  int dead;
  char symbol;
} turn_t;

static int32_t turn_cmp(const void *key, const void *with) {
  ((turn_t *) key)->next_turn=100/((turn_t *) key)->speed+((turn_t *) key)->current_turn;
  ((turn_t *) with)->next_turn=100/((turn_t *) with)->speed+ ((turn_t *) with)->current_turn;
  if(((turn_t *) key)->next_turn - ((turn_t *) with)->next_turn!=0)
	return ((turn_t *) key)->next_turn - ((turn_t *) with)->next_turn;
  else return ((turn_t *) key)->sequence_number - ((turn_t *) with)->sequence_number;
}

static int32_t corridor_path_cmp(const void *key, const void *with) {
  return ((dijkstra_t *) key)->cost - ((dijkstra_t *) with)->cost;
}

int32_t PATH[NUMROWS][NUMCOLS];
int32_t PATH_DIG[NUMROWS][NUMCOLS];

uint32_t num_rooms;
room_t rooms[NUM_OF_ROOMS];

char DUNGEON[NUMROWS][NUMCOLS];
uint8_t HARDNESS[NUMROWS][NUMCOLS];
terrain_type_t map[NUMROWS][NUMCOLS];
//turn_t *CHARACTER_MAP[NUMROWS][NUMCOLS];

int PC_X, PC_Y;
int TURN=TRUE;
int NUM_OF_MONSTERS=15;
turn_t turns[MAX_NUM_MONSTERS];

int NUM_LIVE_MONSTERS=15;

typedef enum action {
  normal,
  load,
  save,
  save_and_load
} action_t;

void display_dungeon();
void draw_character(int x, int y, char use);
void display_monster_list();
void display_all();

void generate_monsters();
void move_queue();
void redraw_dungeon();
int can_see(int x, int y);

void dijkstras(int x_booty, int y_booty);
void dijkstras_dig(int x_booty, int y_booty);
void print_distances();

void generate_dungeon();

void load_dungeon(FILE *file);
void save_dungeon(FILE *file);
void print_dungeon();

void print_hardness();
void clear_dungeon();
int canPlace(int col, int row, int howLargeCol, int howLargeRow);

int main(int argc, char* argv[]) {
	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr, TRUE);
//	keypad(stdscr, FALSE);
	refresh();

	action_t action = normal;
	char* home = getenv("HOME");
	if(home==NULL) return -1;
	strcat(home, "/.rlg327/");
	strcat(home, "dungeon");

	FILE *f;

	//one argument
	if(argc==2){
		if(strcmp(argv[1], "--save")==0){
			action = save;
			f=fopen(home, "wb");
		}
		else if(strcmp(argv[1], "--load")==0){
			action = load;
			f=fopen(home, "rb");
		}
	}
	//two arguments
	else if(argc==3){
		if(strcmp(argv[1], "--save")==0 && strcmp(argv[2], "--load")==0){
			action = save_and_load;
			f=fopen(home, "r+b");
		}
		else if(strcmp(argv[1], "--load")==0 && strcmp(argv[2], "--save")==0){
			action = save_and_load;
			f=fopen(home, "r+b");
		}
		else if(strcmp(argv[1], "--nummon")==0){
			NUM_OF_MONSTERS = atoi(argv[2]);
			NUM_LIVE_MONSTERS = NUM_OF_MONSTERS;
		}
	}

	switch(action){
	case normal:
		generate_dungeon();
		break;
	case load:
		load_dungeon(f);
		fclose(f);
		break;
	case save:
		generate_dungeon();
		save_dungeon(f);
		fclose(f);
		break;
	case save_and_load:
		load_dungeon(f);
		save_dungeon(f);
		fclose(f);
		break;
	}

	dijkstras(PC_X, PC_Y);
	dijkstras_dig(PC_X, PC_Y);

	generate_monsters();

	move_queue();

	printf("YOU (MOST LIKELY) LOSE\n");
	return 0;
}
/*
 * *** Curses (very punny name, "cursor optimization") abstract the terminal from the programmer
 *     We'll use Ncurses (new curses)
 *     To get data types: #include <ncurses.h>
 *     To link in library: -lncurses
 *
 * *** Initialization:
 *     WINDOW *initscr(void)
 *       Initializes the terminal
 *     int raw(void)
 *       Turns off buffered I/O
 *       We can read one byte at a time, no need to wait for user to hit enter
 *     int noecho(void)
 *       Doesn't echo input to the terminal
 *     int curs_set(int visibility)
 *       Make cursor invisible with visibility of 0
 *     int keypad(WINDOW *win, bool bf)
 *       Turns on the keypad
 *
 * My solution to 1.05 uses:
 *      void io_init_terminal(void)
 *      {
 *           initscr();
 *           raw();
 *           noecho();
 *           curs_set(0);
 *           keypad(stdscr, TRUE);
 *      }
 *
 * *** Deinitialization
 *     int endwin(void)
 *       Returns resourses to the system
 *     If your program crashes and corrupts the terminal: run the "reset" command
 *
 * *** Reading input
 *     int getch(void)
 *       Read a single character
 *       blocking call
 *
 * *** Displaying
 *     int clear(void)
 *       Before redrawing, use this
 *     int mvaddch(int y, int x, const chtype ch)
 *       Move cursor to (y, x) and write ch
 *     int refresh(void)
 *       Redraw the terminal
 *
 * *** Other I/O
 *     See chrs_addch(3), curs_printw(3), curs_getch(3) and curs_scanw(3)
 *
 * *** More info
 *     Fairly comprehensive coverage in curses(3), including a list of all curses functions.
 *     Lots of curses tutorials and quick-start guides on the web.
 *     Check out ncurses.h (in /usr/include/ncurses.h on pyrite).
 */

void draw_character(int x, int y, char use){
	mvaddch(y+1, x, use);
	refresh();
}
void display_dungeon(){
	int i, j;
	for(i=0;i<NUMROWS;i++){
		for(j=0;j<NUMCOLS;j++){
			draw_character(j, i, DUNGEON[i][j]);
		}
	}
}
void display_all(){
	int x, y, i;
	for(y=0;y<NUMROWS;y++){
		for(x=0;x<NUMCOLS;x++){
			int is_monster=FALSE;
			for(i=1; i<NUM_OF_MONSTERS+1; i++){
				if(turns[i].pos[dim_y]==y && turns[i].pos[dim_x]==x && turns[i].dead==FALSE){
					draw_character(x, y, turns[i].symbol);
					is_monster=TRUE; break;
				}
			}
			if(is_monster==FALSE&& !(y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x]))
				draw_character(x, y, DUNGEON[y][x]);
			else if((y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x])&&is_monster==FALSE){
				draw_character(x, y, PC);
			}
		}
	}
}
void display_monster_list(){
	int i, x, y, index;
	int start=1;
	for(i=24; i<NUMCOLS-26; i++) draw_character(i, 4, '-');
	for(i=24; i<NUMCOLS-26; i++) draw_character(i, NUMROWS-5, '-');
	for(i=4; i<NUMROWS-4; i++) draw_character(24, i, '|');
	for(i=4; i<NUMROWS-4; i++) draw_character(NUMCOLS-26, i, '|');

	while(1){
		index=start; i=5;
		while(index<=NUM_OF_MONSTERS){
			if(i<NUMROWS-5 && turns[index].dead==FALSE){
				x = turns[0].pos[dim_x]-turns[index].pos[dim_x];
				y = turns[0].pos[dim_y]-turns[index].pos[dim_y];

				//NORTH WEST
				if(x>0 && y>0) mvprintw(i+1, 25, "%3d. %c, %2d north and %2d west ", index, turns[index].symbol, y, x);
				//SOUTH WEST
				else if(x<0 && y>0) mvprintw(i+1, 25, "%3d. %c, %2d south and %2d west ", index, turns[index].symbol, y, abs(x));
				//NORTH EAST
				else if(x>0 && y<0) mvprintw(i+1, 25, "%3d. %c, %2d north and %2d east ", index, turns[index].symbol, abs(y), x);
				//SOUTH EAST
				else mvprintw(i+1, 25, "%3d. %c, %2d south and %2d east ", index, turns[index].symbol, abs(y), abs(x));
				i++;
			}
			index++;
		}
		int ch = getch();
		//When displaying monster list, if entire list does not fit in screen and not currently at top of
		//list, scroll list up.
		if(ch==KEY_UP && NUM_LIVE_MONSTERS>NUMROWS-10 && start>=2)
			start--;

		//When displaying monster list, if entire list does not fit in screen and not currently at bottom
		//of list, scroll list down.
		else if(ch==KEY_DOWN && NUM_LIVE_MONSTERS>NUMROWS-10 && NUM_LIVE_MONSTERS-start>NUMROWS-10)
			start++;

		//When displaying monster list, return to character control.
		else if(ch==27){
			for(y=4;y<NUMROWS-4;y++){
				for(x=24;x<NUMCOLS-25;x++){
					int is_monster=FALSE;
					for(i=1; i<NUM_OF_MONSTERS+1; i++){
						if(turns[i].pos[dim_y]==y && turns[i].pos[dim_x]==x && turns[i].dead==FALSE){
							draw_character(x, y, turns[i].symbol);
							is_monster=TRUE; break;
						}
					}
					if(is_monster==FALSE&& !(y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x]))
						draw_character(x, y, DUNGEON[y][x]);
					else if((y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x])&&is_monster==FALSE){
						draw_character(x, y, PC);
					}
				}
			}
			break;
		}
	}
}

/*
 * Each character moves every floor(100/speed) turns (integer math, so this is just a normal division).
 * The game turn starts at zero, as do all characters’ first moves, and advances to the value at the front
 * of the priority queue every time it is dequeued. A system built with this kind of priority queue drive
 * mechanism is known as a discrete event simulator. It’s not actually necessary to keep track of the game
 * turn. Since the priority queue is sorted by game turn, the game turn is, by definition, the next turn
 * of the character at the front of the queue.
 */
void move_queue(){
	static uint32_t initialized = 0;
	turn_t *p; heap_t h; int i, x, y;
	int generate_new=FALSE;

	if (!initialized) {
		for (i = 0; i < NUM_OF_MONSTERS + 1; i++) {
			turns[i].current_turn = 0;
		}
		initialized = 1;
	}
	heap_init(&h, turn_cmp, NULL);

	//chars positions
	for (i = 0; i < NUM_OF_MONSTERS+1; i++) {
		turns[i].hn = heap_insert(&h, &turns[i]);
	}
	int abort=FALSE;
	//TURN
	while((p = heap_remove_min(&h))){
		p->hn = NULL;
		if(turns[p->sequence_number].dead==FALSE){
			if(p->sequence_number==0){ //PC MOVE
				turns[p->sequence_number].current_turn=p->current_turn+1;
				int ch;
				int tempx = turns[0].pos[dim_x];
				int tempy = turns[0].pos[dim_y];

				while(1){
					ch = getch();
					//Attempt to move PC one cell to the upper left.
					if(ch=='7'||ch=='y'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx-1; tempy=tempy-1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell up.
					else if(ch=='8'||ch=='k'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempy=tempy-1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell to the upper right.
					else if(ch=='9'||ch=='u'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx+1; tempy=tempy-1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell to the right.
					else if(ch=='6'||ch=='l'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx+1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell to the lower right.
					else if(ch=='3'||ch=='n'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx+1; tempy=tempy+1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell down.
					else if(ch=='2'||ch=='j'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempy=tempy+1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell to the lower left.
					else if(ch=='1'||ch=='b'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx-1; tempy=tempy+1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Attempt to move PC one cell to the left.
					else if(ch=='4'||ch=='h'){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						tempx=tempx-1;
						if(map[tempy][tempx]!=ter_wall_immutable && map[tempy][tempx]!=ter_wall){
							turns[0].pos[dim_x]=tempx;
							turns[0].pos[dim_y]=tempy;
						} break;
					}
					//Rest for a turn. NPCs still move
					else if(ch==' '){
						draw_character(tempx, tempy, DUNGEON[tempy][tempx]);
						break;
					}
					//Display a list of monsters in the dungeon, with their symbol and position relative to the PC
					//(e.g.: “c, 2 north and 14 west”).
					else if(ch=='m'){
						display_monster_list();
					}
					//Save to disk and exit game. Saving and restoring will be revisited later. For now, this will
					//just quit the game.
					else if(ch=='s'||ch=='S'){
						abort=TRUE;
						break;
					}
				}
				if(abort==TRUE){
					endwin();
					break;
				}
				dijkstras(PC_X, turns[0].pos[dim_y]);
				dijkstras_dig(PC_X, turns[0].pos[dim_y]);
				draw_character(turns[p->sequence_number].pos[dim_x], turns[p->sequence_number].pos[dim_y], turns[p->sequence_number].symbol);
			}
			else{ //MONSTER MOVE
				draw_character(turns[p->sequence_number].pos[dim_x], turns[p->sequence_number].pos[dim_y], DUNGEON[turns[p->sequence_number].pos[dim_y]][turns[p->sequence_number].pos[dim_x]]);
				int tempx=turns[p->sequence_number].pos[dim_x];
				int tempy=turns[p->sequence_number].pos[dim_y];
				turns[p->sequence_number].current_turn=p->current_turn+1;

				//ERRATIC 50% CHANCE
				if(((turns[p->sequence_number].ability&(NPC_ERRATIC))==(NPC_ERRATIC)) && rand()%2==1){
					//CAN TUNNEL
					if((turns[p->sequence_number].ability&(NPC_TUNNEL)) == (NPC_TUNNEL)){
						int can_move=FALSE;
						while(can_move==FALSE){
							x = tempx + rand()%3-1;
							y = tempy + rand()%3-1;
							if(map[y][x]!=ter_wall_immutable){
								if(DUNGEON[y][x]==ROCK){
									if(HARDNESS[y][x]-85<=0){
										HARDNESS[y][x]=0;
										DUNGEON[y][x]=CORRIDOR;
										map[y][x]= ter_floor_hall;
									}else{
										HARDNESS[y][x]=HARDNESS[y][x]-85;
										x = tempx; y = tempy;
									}
								}
								turns[p->sequence_number].pos[dim_x]=x;
								turns[p->sequence_number].pos[dim_y]=y;
								can_move=TRUE;
							}
						}
					}
					//CANT TUNNEL
					else{
						int can_move=FALSE;
						while(can_move==FALSE){
							x = tempx + rand()%3-1;
							y = tempy + rand()%3-1;
							if(map[y][x]!=ter_wall_immutable && map[y][x]!=ter_wall){
								turns[p->sequence_number].pos[dim_x]=x;
								turns[p->sequence_number].pos[dim_y]=y;
								can_move=TRUE;
							}
						}
					}
				}
				//TUNNEL
				else if((turns[p->sequence_number].ability&(NPC_TUNNEL)) == (NPC_TUNNEL)){
					//TELEPATHETIC & INTELLIGENT
					if((turns[p->sequence_number].ability&(NPC_TELEPATHIC|NPC_INTELLIGENT)) == (NPC_TELEPATHIC|NPC_INTELLIGENT)){
						x=turns[p->sequence_number].pos[dim_x];
						y=turns[p->sequence_number].pos[dim_y];

						int current = PATH_DIG[y][x];
						if (PATH_DIG[y-1][x]<current){//checks up
							tempy=y-1; tempx=x; current=PATH_DIG[y-1][x];
						}
						if (PATH_DIG[y][x-1]<current){//checks left
							tempy=y; tempx=x-1; current=PATH_DIG[y][x-1];
						}
						if (PATH_DIG[y][x+1]<current){//checks right
							tempy=y; tempx=x+1; current=PATH_DIG[y][x+1];
						}
						if (PATH_DIG[y+1][x]<current){//checks down
							tempy=y+1; tempx=x; current=PATH_DIG[y+1][x];
						}
						if (PATH_DIG[y+1][x+1]<current){//checks down/right
							tempy=y+1; tempx=x+1; current=PATH_DIG[y+1][x+1];
						}
						if (PATH_DIG[y+1][x-1]<current){//checks down/left
							tempy=y+1; tempx=x-1; current=PATH_DIG[y+1][x-1];
						}
						if (PATH_DIG[y-1][x+1]<current){//checks up/right
							tempy=y-1; tempx=x+1; current=PATH_DIG[y-1][x+1];
						}
						if (PATH_DIG[y-1][x-1]<current){//checks up/left
							tempy=y-1; tempx=x-1; current=PATH_DIG[y-1][x-1];
						}
					}
					//extremely TELEPATHETIC
					else if((turns[p->sequence_number].ability&(NPC_TELEPATHIC))==(NPC_TELEPATHIC)){
						if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]>0){
							tempx=turns[p->sequence_number].pos[dim_x]+1;
						}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]<0){
							tempx=turns[p->sequence_number].pos[dim_x]-1;
						}
						if(turns[0].pos[dim_y]-turns[p->sequence_number].pos[dim_y]>0){
							tempy=turns[p->sequence_number].pos[dim_y]+1;
						}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_y]<0){
							tempy=turns[p->sequence_number].pos[dim_y]-1;
						}
					}
					//INTELLIGENT
					else if((turns[p->sequence_number].ability&(NPC_INTELLIGENT))==(NPC_INTELLIGENT)){
						if(can_see(tempx, tempy)==TRUE){
							turns[p->sequence_number].booty[dim_x]=turns[0].pos[dim_x];
							turns[p->sequence_number].booty[dim_y]=turns[0].pos[dim_y];
						}
						if(turns[p->sequence_number].booty[dim_y]!=0 && turns[p->sequence_number].booty[dim_x]!=0){
							//int x_booty, int y_booty
							dijkstras(turns[p->sequence_number].booty[dim_x], turns[p->sequence_number].booty[dim_y]);
							x=turns[p->sequence_number].pos[dim_x];
							y=turns[p->sequence_number].pos[dim_y];

							int current = PATH_DIG[y][x];
							if (PATH_DIG[y-1][x]<current){//checks up
								tempy=y-1; tempx=x; current=PATH_DIG[y-1][x];
							}
							if (PATH_DIG[y][x-1]<current){//checks left
								tempy=y; tempx=x-1; current=PATH_DIG[y][x-1];
							}
							if (PATH_DIG[y][x+1]<current){//checks right
								tempy=y; tempx=x+1; current=PATH_DIG[y][x+1];
							}
							if (PATH_DIG[y+1][x]<current){//checks down
								tempy=y+1; tempx=x; current=PATH_DIG[y+1][x];
							}
							if (PATH_DIG[y+1][x+1]<current){//checks down/right
								tempy=y+1; tempx=x+1; current=PATH_DIG[y+1][x+1];
							}
							if (PATH_DIG[y+1][x-1]<current){//checks down/left
								tempy=y+1; tempx=x-1; current=PATH_DIG[y+1][x-1];
							}
							if (PATH_DIG[y-1][x+1]<current){//checks up/right
								tempy=y-1; tempx=x+1; current=PATH_DIG[y-1][x+1];
							}
							if (PATH_DIG[y-1][x-1]<current){//checks up/left
								tempy=y-1; tempx=x-1; current=PATH_DIG[y-1][x-1];
							}
						}
					}
					else{ //NO OTHER ABILITIES
						if(can_see(tempx, tempy)==TRUE){
							if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]>0){
								tempx=turns[p->sequence_number].pos[dim_x]+1;
							}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]<0){
								tempx=turns[p->sequence_number].pos[dim_x]-1;
							}
							if(turns[0].pos[dim_y]-turns[p->sequence_number].pos[dim_y]>0){
								tempy=turns[p->sequence_number].pos[dim_y]+1;
							}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_y]<0){
								tempy=turns[p->sequence_number].pos[dim_y]-1;
							}
						}
						else{
							int can_move=FALSE;
							while(can_move==FALSE){
								x = tempx + rand()%3-1;
								y = tempy + rand()%3-1;
								if(map[y][x]!=ter_wall_immutable){
									can_move=TRUE;
									tempx=x; tempy=y;
								}
							}
						}
					}

					if(map[tempy][tempx]== ter_wall){
						if(HARDNESS[tempy][tempx]-85<=0){
							HARDNESS[tempy][tempx]=0;
							DUNGEON[tempy][tempx]=CORRIDOR;
							map[tempy][tempx]= ter_floor_hall;
						}else{
							HARDNESS[tempy][tempx]=HARDNESS[y][x]-85;
							tempx=turns[p->sequence_number].pos[dim_x];
							tempy=turns[p->sequence_number].pos[dim_y];
						}
					}
					turns[p->sequence_number].pos[dim_x]=tempx;
					turns[p->sequence_number].pos[dim_y]=tempy;
				}
				//CAN'T TUNNEL
				else{
					//TELEPATHETIC & INTELLIGENT
					if((turns[p->sequence_number].ability&(NPC_TELEPATHIC|NPC_INTELLIGENT)) ==(NPC_TELEPATHIC|NPC_INTELLIGENT)){
						x=turns[p->sequence_number].pos[dim_x];
						y=turns[p->sequence_number].pos[dim_y];

						int current = PATH[y][x];
						if (PATH[y-1][x]<current){//checks up
							tempy=y-1; tempx=x; current=PATH[y-1][x];
						}
						if (PATH[y][x-1]<current){//checks left
							tempy=y; tempx=x-1; current=PATH[y][x-1];
						}
						if (PATH[y][x+1]<current){//checks right
							tempy=y; tempx=x+1; current=PATH[y][x+1];
						}
						if (PATH[y+1][x]<current){//checks down
							tempy=y+1; tempx=x; current=PATH[y+1][x];
						}
						if (PATH[y+1][x+1]<current){//checks down/right
							tempy=y+1; tempx=x+1; current=PATH[y+1][x+1];
						}
						if (PATH[y+1][x-1]<current){//checks down/left
							tempy=y+1; tempx=x-1; current=PATH[y+1][x-1];
						}
						if (PATH[y-1][x+1]<current){//checks up/right
							tempy=y-1; tempx=x+1; current=PATH[y-1][x+1];
						}
						if (PATH[y-1][x-1]<current){//checks up/left
							tempy=y-1; tempx=x-1; current=PATH[y-1][x-1];
						}
						turns[p->sequence_number].pos[dim_x]=tempx;
						turns[p->sequence_number].pos[dim_y]=tempy;
					}
					//extremely TELEPATHETIC
					else if((turns[p->sequence_number].ability&(NPC_TELEPATHIC))==(NPC_TELEPATHIC)){
						if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]>0){
							tempx=turns[p->sequence_number].pos[dim_x]+1;
						}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]<0){
							tempx=turns[p->sequence_number].pos[dim_x]-1;
						}
						if(turns[0].pos[dim_y]-turns[p->sequence_number].pos[dim_y]>0){
							tempy=turns[p->sequence_number].pos[dim_y]+1;
						}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_y]<0){
							tempy=turns[p->sequence_number].pos[dim_y]-1;
						}
						if(map[tempy][tempx]!=ter_wall&&map[tempy][tempx]!=ter_wall_immutable){
							turns[p->sequence_number].pos[dim_x]=tempx;
							turns[p->sequence_number].pos[dim_y]=tempy;
						}
					}
					//INTELLIGENT
					else if((turns[p->sequence_number].ability&(NPC_INTELLIGENT))==(NPC_INTELLIGENT)){
						if(can_see(tempx, tempy)==TRUE){
							turns[p->sequence_number].booty[dim_x]=turns[0].pos[dim_x];
							turns[p->sequence_number].booty[dim_y]=turns[0].pos[dim_y];
						}
						if(turns[p->sequence_number].booty[dim_x]!=0 && turns[p->sequence_number].booty[dim_y]!=0){
							//int x_booty, int y_booty
							dijkstras(turns[p->sequence_number].booty[dim_x], turns[p->sequence_number].booty[dim_y]);
							x=turns[p->sequence_number].pos[dim_x];
							y=turns[p->sequence_number].pos[dim_y];

							int current = PATH[y][x];
							if (PATH[y-1][x]<current){//checks up
								tempy=y-1; tempx=x; current=PATH[y-1][x];
							}
							if (PATH[y][x-1]<current){//checks left
								tempy=y; tempx=x-1; current=PATH[y][x-1];
							}
							if (PATH[y][x+1]<current){//checks right
								tempy=y; tempx=x+1; current=PATH[y][x+1];
							}
							if (PATH[y+1][x]<current){//checks down
								tempy=y+1; tempx=x; current=PATH[y+1][x];
							}
							if (PATH[y+1][x+1]<current){//checks down/right
								tempy=y+1; tempx=x+1; current=PATH[y+1][x+1];
							}
							if (PATH[y+1][x-1]<current){//checks down/left
								tempy=y+1; tempx=x-1; current=PATH[y+1][x-1];
							}
							if (PATH[y-1][x+1]<current){//checks up/right
								tempy=y-1; tempx=x+1; current=PATH[y-1][x+1];
							}
							if (PATH[y-1][x-1]<current){//checks up/left
								tempy=y-1; tempx=x-1; current=PATH[y-1][x-1];
							}
							turns[p->sequence_number].pos[dim_x]=tempx;
							turns[p->sequence_number].pos[dim_y]=tempy;
						}
					}
					//NO ABILITIES
					else{
						if(can_see(tempx, tempy)==TRUE){
							if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]>0){
								tempx=turns[p->sequence_number].pos[dim_x]+1;
							}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_x]<0){
								tempx=turns[p->sequence_number].pos[dim_x]-1;
							}
							if(turns[0].pos[dim_y]-turns[p->sequence_number].pos[dim_y]>0){
								tempy=turns[p->sequence_number].pos[dim_y]+1;
							}else if(turns[0].pos[dim_x]-turns[p->sequence_number].pos[dim_y]<0){
								tempy=turns[p->sequence_number].pos[dim_y]-1;
							}
						}
						else{
							int can_move=FALSE;
							while(can_move==FALSE){
								x = tempx + rand()%3-1;
								y = tempy + rand()%3-1;
								if(map[y][x]!=ter_wall_immutable && map[y][x]!=ter_wall){
									tempx=x; tempy=y;
									can_move=TRUE;
								}
							}
						}
						if(map[tempy][tempx]!=ter_wall&&map[tempy][tempx]!=ter_wall_immutable){
							turns[p->sequence_number].pos[dim_x]=tempx;
							turns[p->sequence_number].pos[dim_y]=tempy;
						}
					}
				}
			}
			if(p->sequence_number!=0 && turns[p->sequence_number].pos[dim_y]==turns[0].pos[dim_y]&&
			turns[p->sequence_number].pos[dim_x]==turns[0].pos[dim_x]){
				endwin();
				break;
			}
			for(i=1;i<NUM_OF_MONSTERS+1;i++){
				if(i != p->sequence_number && turns[p->sequence_number].pos[dim_y]==turns[i].pos[dim_y] &&
				turns[p->sequence_number].pos[dim_x]==turns[i].pos[dim_x]){
					NUM_LIVE_MONSTERS--;
					turns[i].dead=TRUE;
				}
			}
			draw_character(turns[p->sequence_number].pos[dim_x], turns[p->sequence_number].pos[dim_y], turns[p->sequence_number].symbol);
			turns[p->sequence_number].hn = heap_insert(&h, &turns[p->sequence_number]);

			if(p->sequence_number==0 && (DUNGEON[turns[p->sequence_number].pos[dim_y]][turns[p->sequence_number].pos[dim_x]]=='<'||
			DUNGEON[turns[p->sequence_number].pos[dim_y]][turns[p->sequence_number].pos[dim_x]]=='>')){
				generate_new=TRUE;
				break;
			}
		}
	}
	heap_delete(&h);
	if(generate_new==TRUE){
		generate_dungeon();
		dijkstras(PC_X, PC_Y);
		dijkstras_dig(PC_X, PC_Y);
		generate_monsters();
		move_queue();
	}
}

void generate_monsters() {
	int i = 1;
	turn_t PC_turn;
	PC_turn.pos[dim_y] = PC_Y;
	PC_turn.pos[dim_x] = PC_X;
	PC_turn.speed = 10;
	PC_turn.sequence_number = 0;
	PC_turn.dead = FALSE;
	PC_turn.symbol=PC;

	turns[0] = PC_turn;
//	CHARACTER_MAP[turns[0].pos[dim_x]][turns[0].pos[dim_y]]=&turns[0];
	draw_character(turns[0].pos[dim_x], turns[0].pos[dim_y], PC);

	while (i < NUM_OF_MONSTERS + 1) {
		turn_t MON_turn;

//		while(1){
			int rand_room = rand()%(NUM_OF_ROOMS-1)+1;
			MON_turn.pos[dim_x] = rooms[rand_room].x_pos + (rand()%rooms[rand_room].width);
			MON_turn.pos[dim_y] = rooms[rand_room].y_pos + (rand()%rooms[rand_room].height);
//			if(CHARACTER_MAP[MON_turn.pos[dim_x]][MON_turn.pos[dim_y]]==NULL) break;
//		}

		MON_turn.speed = rand() % (15) + 5;
		MON_turn.sequence_number = i;
		MON_turn.dead = FALSE;

		MON_turn.booty[dim_x] = 0;	MON_turn.booty[dim_y] = 0;

		MON_turn.ability = 0b0000;
		if (rand() % 2) MON_turn.ability = MON_turn.ability | NPC_INTELLIGENT;
		if (rand() % 2) MON_turn.ability = MON_turn.ability | NPC_TELEPATHIC;
		if (rand() % 2) MON_turn.ability = MON_turn.ability | NPC_TUNNEL;
		if (rand() % 2) MON_turn.ability = MON_turn.ability | NPC_ERRATIC;

		//1111
		if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC)) == (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC))
			MON_turn.symbol = '1';
		//0111
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL)) == (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL))
			MON_turn.symbol = '2';
		//1011
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_ERRATIC)) == (NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_ERRATIC))
			MON_turn.symbol = '3';
		//1101
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TUNNEL | NPC_ERRATIC)) == (NPC_INTELLIGENT | NPC_TUNNEL | NPC_ERRATIC))
			MON_turn.symbol = '4';
		//1110
		else if ((MON_turn.ability & (NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC)) == (NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC))
			MON_turn.symbol = '5';
		//0011
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TELEPATHIC)) == (NPC_INTELLIGENT | NPC_TELEPATHIC))
			MON_turn.symbol = '6';
		//1001
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_ERRATIC)) == (NPC_INTELLIGENT | NPC_ERRATIC))
			MON_turn.symbol = '7';
		//0101
		else if ((MON_turn.ability & (NPC_INTELLIGENT | NPC_TUNNEL)) == (NPC_INTELLIGENT | NPC_TUNNEL))
			MON_turn.symbol = '8';
		//1010
		else if ((MON_turn.ability & (NPC_TELEPATHIC | NPC_ERRATIC)) == (NPC_TELEPATHIC | NPC_ERRATIC))
			MON_turn.symbol = '9';
		//0110
		else if ((MON_turn.ability & (NPC_TELEPATHIC | NPC_TUNNEL)) == (NPC_TELEPATHIC | NPC_TUNNEL))
			MON_turn.symbol = 'a';
		//0011
		else if ((MON_turn.ability & (NPC_TUNNEL | NPC_ERRATIC))== (NPC_TUNNEL | NPC_ERRATIC))
			MON_turn.symbol = 'b';
		//0001
		else if ((MON_turn.ability & (NPC_INTELLIGENT)) == (NPC_INTELLIGENT))
			MON_turn.symbol = 'c';
		//0010
		else if ((MON_turn.ability & (NPC_TELEPATHIC)) == (NPC_TELEPATHIC))
			MON_turn.symbol = 'd';
		//1000
		else if ((MON_turn.ability & (NPC_ERRATIC)) == (NPC_ERRATIC))
			MON_turn.symbol = 'e';
		//0100
		else if ((MON_turn.ability & (NPC_TUNNEL)) == (NPC_TUNNEL))
			MON_turn.symbol = 'f';
		//0000
		else if ((MON_turn.ability & (!(NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC)))== (!(NPC_INTELLIGENT | NPC_TELEPATHIC | NPC_TUNNEL | NPC_ERRATIC)))
			MON_turn.symbol = '0';

		turns[i] = MON_turn;
//		CHARACTER_MAP[turns[i].pos[dim_x]][turns[i].pos[dim_y]]=&turns[i];
		draw_character(turns[i].pos[dim_x], turns[i].pos[dim_y], turns[i].symbol);

		i++;
	}
}
int can_see(int mons_x, int mons_y) {
	int dx, dy, p, end;
	float x1, x2, y1, y2, x, y;
	x1 = mons_x; y1 = mons_y;
	x2 = turns[0].pos[dim_x];
	y2 = turns[0].pos[dim_y];

	dx = abs(x1 - x2);
	dy = abs(y1 - y2);
	p = 2 * dy - dx;

	if (x1 > x2) {
		x = x2; y = y2; end = x1;
	} else {
		x = x1; y = y1; end = x2;
	}
	while (x < end) {
		x = x + 1;
		if (p < 0) p = p + 2 * dy;
		else {
			y = y + 1;
			p = p + 2 * (dy - dx);
		}
		if (map[(int) y][(int) x] == ter_wall || map[(int) y][(int) x] == ter_wall_immutable)
			return FALSE;
	}
	return TRUE;
}
void redraw_dungeon(){
	int x, y, i;
//	for(y=0;y<82;y++) printf("-");
//	printf("\n");
	for(y=0;y<NUMROWS;y++){
//		printf("|");
		for(x=0;x<NUMCOLS;x++){
			int is_monster=FALSE;
			for(i=1;i<NUM_OF_MONSTERS+1;i++){
				if(turns[i].pos[dim_y]==y && turns[i].pos[dim_x]==x && turns[i].dead==FALSE){
					is_monster=TRUE;
					printf(ANSI_COLOR_MAGENTA     "%c"     ANSI_COLOR_RESET, turns[i].symbol);
					break;
				}
			}
			if(is_monster==FALSE&& !(y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x]))
				printf("%c", DUNGEON[y][x]);
			else if((y==turns[0].pos[dim_y] && x==turns[0].pos[dim_x])&&is_monster==FALSE)
				printf(ANSI_COLOR_YELLOW		"%c"	 ANSI_COLOR_RESET, PC);
		}
//		printf("|\n");
	}
//	for(y=0;y<82;y++) printf("-");
	printf("\n");
	usleep(250000);
}

/*
 * For the non-tunneling monsters, we’ll give a weight of 1 for floor and ignore wall cells
 * (i.e., don’t try to find paths through walls).
 */
void dijkstras(int x_booty, int y_booty){
	dijkstra_t path[NUMROWS][NUMCOLS], *p;
	uint32_t x, y;
	heap_t h;
	int i, j;

	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			path[y][x].pos[dim_y] = y;
			path[y][x].pos[dim_x] = x;
		}
	}

	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			path[y][x].cost = 255;
		}
	}
	path[y_booty][x_booty].cost = 0;
	PATH[y_booty][x_booty]=0;
	heap_init(&h, corridor_path_cmp, NULL);

	//chars positions
	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			if (map[y][x] != ter_wall_immutable) path[y][x].hn = heap_insert(&h, &path[y][x]);
			else path[y][x].hn = NULL;
		}
	}

	//checks neighbors
	while((p = heap_remove_min(&h))){
		p->hn = NULL;
		//checks up
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x];
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks left
		i=p->pos[dim_y];
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks right
		i=p->pos[dim_y];
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x];
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down/right
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down/left
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks up/right
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks up/left
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && (map[i][j]==ter_floor_room || map[i][j]==ter_floor_hall)) {
			path[i][j].cost = p->cost + 1;
			PATH[i][j]=path[i][j].cost;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
	}
	heap_delete(&h);
}
/*
 * For the tunnelers, we’ll have to use weights based on the hardness; cells with a hardness of 0
 * have a weight of 1, and cells with hardnesses in the ranges [1; 84]; [85; 170], and [171; 254]
 * have weights of 1, 2, and 3, respectively. A hardness of 255 has infinite weight. We don’t have
 * to assign a value to this. Instead, we simply do not put it in the queue.
 */
void dijkstras_dig(int x_booty, int y_booty){
	dijkstra_t path[NUMROWS][NUMCOLS], *p;
	uint32_t x, y, num;
	heap_t h;
	int i, j;

	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			path[y][x].pos[dim_y] = y;
			path[y][x].pos[dim_x] = x;
		}
	}

	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			path[y][x].cost = 255;
		}
	}
	path[y_booty][x_booty].cost = 0;
	PATH_DIG[y_booty][x_booty]=0;

	heap_init(&h, corridor_path_cmp, NULL);

	//chars positions
	for (y = 0; y < NUMROWS; y++) {
		for (x = 0; x < NUMCOLS; x++) {
			if (map[y][x] != ter_wall_immutable) path[y][x].hn = heap_insert(&h, &path[y][x]);
			else path[y][x].hn = NULL;
		}
	}
	//checks neighbors
	while((p = heap_remove_min(&h))){
		p->hn = NULL;

		//checks up
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x];
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks left
		i=p->pos[dim_y];
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks right
		i=p->pos[dim_y];
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x];
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down/right
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks down/left
		i=p->pos[dim_y] + 1;
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks up/right
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x] + 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
		//checks up/left
		i=p->pos[dim_y] - 1;
		j=p->pos[dim_x] - 1;
		if ((path[i][j].hn) && (path[i][j].cost> p->cost) && map[i][j]!=ter_wall_immutable ) {
			path[i][j].cost = p->cost + 1;
			if(HARDNESS[i][j]==0) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=1 && HARDNESS[i][j]<=84) num=path[i][j].cost;
			else if(HARDNESS[i][j]>=85 && HARDNESS[i][j]<=170) num=path[i][j].cost + 1;
			else num=path[i][j].cost + 2;
			PATH_DIG[i][j]=num;

			heap_decrease_key_no_replace(&h, path[i][j].hn);
		}
	}
	heap_delete(&h);
}
/*
 * The load switch will load the dungeon from disk, rather than generate a new one, then display
 * it and exit.
 */
void load_dungeon(FILE *file){
	int i, j, k;
	char charbuffer[6];
	unsigned int intbuffer[2];

	//reads file name, file version, and file size
	fread(charbuffer, sizeof(char), 6, file);
	fread(intbuffer, sizeof(int), 2, file);
	intbuffer[0]=htobe32(intbuffer[0]);
	intbuffer[1]=htobe32(intbuffer[1]);
	int filesize=intbuffer[1];

	unsigned char buffy[1482];

	//reads hardness
	fread(buffy, sizeof(char), 1482, file);

	//interprets hardness from buffy and makes corridors
	k=0;
	for(i=0;i<NUMROWS;i++){
		for(j=0;j<NUMCOLS;j++){
			if(i==0||j==0||i==NUMROWS-1||j==NUMCOLS-1){
				DUNGEON[i][j]=ROCK;
				HARDNESS[i][j]=255;
				map[i][j] = ter_wall_immutable;
			}
			else if(buffy[k]==0){
				DUNGEON[i][j]=CORRIDOR;
				HARDNESS[i][j]=0;
				map[i][j] = ter_floor_hall;
				k++;
			}
			else if(buffy[k]!=0){
				DUNGEON[i][j]=ROCK;
				HARDNESS[i][j]=buffy[k];
				map[i][j]= ter_wall;
				k++;
			}
		}
	}

	unsigned char room[4];

	int numrooms=(filesize-1496)/4;

	//creates rooms from the positions given at the end of the file
	for(k=0; k<numrooms; k++){
		fread(room, sizeof(char), 4, file);
		int x_pos = room[0];
		int y_pos = room[1];
		int width = room[2];
		int height = room[3];
		for(i=y_pos;i<height+y_pos;i++){
			for(j=x_pos;j<width+x_pos;j++){
				DUNGEON[i][j]=ROOM;
				HARDNESS[i][j]=0;
				map[i][j]= ter_floor_room;
			}
		}
	}
}
/*
 * The save switch will cause the game to save the dungeon to disk before terminating.
 */
void save_dungeon(FILE *file){
	char charbuffer[6]="RLG327";
	unsigned int intbuffer[2];
	int i=0;
	int k, j;

	int numroomdata = num_rooms*4;
	int filesize=1496+numroomdata;

	intbuffer[0]=htobe32(i);
	intbuffer[1]=htobe32(filesize);

	//writes file name, file version, and file size
	fwrite(charbuffer, sizeof(char), 6, file);
	fwrite(intbuffer, sizeof(int), 2, file);

	unsigned char buffy[1482];

	k=0;
	for(i=0;i<NUMROWS;i++){
		for(j=0;j<NUMCOLS;j++){
			if( i!=0&&j!=0 && i!=NUMROWS-1 && j!=NUMCOLS-1 ){
				buffy[k]=HARDNESS[i][j];
				k++;
			}
		}
	}
	fwrite(buffy, sizeof(char), 1482, file);

	unsigned char roomstowrite[4];

	for(k=0; k<num_rooms; k++){
		roomstowrite[0]=rooms[k].x_pos;
		roomstowrite[1]=rooms[k].y_pos;
		roomstowrite[2]=rooms[k].width;
		roomstowrite[3]=rooms[k].height;
		fwrite(roomstowrite, sizeof(char), 4, file);
	}
}

void clear_dungeon(){
	int i, j;
	for(i=0;i<NUMROWS;i++){
		for(j=0;j<NUMCOLS;j++){
			DUNGEON[i][j]=ROCK;
			HARDNESS[i][j]=rand()%(253)+1;//range(1-254)
			map[i][j]= ter_wall;
			if (i == 0 || i == NUMROWS - 1 ||
				  j == 0 || j == NUMCOLS - 1) {
				map[i][j] = ter_wall_immutable;
				HARDNESS[i][j] = 255;
			}
			PATH[i][j]=255;
			PATH_DIG[i][j]=255;
//			turn_map[i][j]=NULL;
		}
	}
}
void print_distances(){
	int i, j;

	for(i=0;i<82;i++) printf("-");
	printf("\n");
	for(i=0;i<NUMROWS;i++){
		printf("|");
		for(j=0;j<NUMCOLS;j++){
			if(PATH[i][j]>=62) printf("%c", DUNGEON[i][j]);
			else if(PATH[i][j]<=9) printf("%c", '0'+PATH[i][j]);
			else if(PATH[i][j]<=35) printf("%c", 'a'+PATH[i][j]-10);
			else if(PATH[i][j]<62) printf("%c", 'A'+PATH[i][j]-36);
		}
		printf("|\n");
	}
	for(i=0;i<82;i++) printf("-");
	printf("\n");

	for(i=0;i<82;i++) printf("-");
	printf("\n");
	for(i=0;i<NUMROWS;i++){
		printf("|");
		for(j=0;j<NUMCOLS;j++){
			if(PATH_DIG[i][j]>=62) printf("%c", DUNGEON[i][j]);
			else if(PATH_DIG[i][j]<=9) printf("%c", '0'+PATH_DIG[i][j]);
			else if(PATH_DIG[i][j]<=35) printf("%c", 'a'+PATH_DIG[i][j]-10);
			else if(PATH_DIG[i][j]<62) printf("%c", 'A'+PATH_DIG[i][j]-36);
		}
		printf("|\n");
	}
	for(i=0;i<82;i++) printf("-");
	printf("\n");
}
void print_dungeon(){
	int i, j;
	for(i=0;i<82;i++) printf("-");
	printf("\n");
	for(i=0;i<NUMROWS;i++){
		printf("|");
		for(j=0;j<NUMCOLS;j++){
			printf("%c", DUNGEON[i][j]);
		}
		printf("|\n");
	}
	for(i=0;i<82;i++) printf("-");
	printf("\n");
}
void print_hardness(){
	int i, j;
	for(i=0;i<82;i++) printf("-");
	printf("\n");
	for(i=0;i<NUMROWS;i++){
		printf("|");
		for(j=0;j<NUMCOLS;j++){
			printf("[%3d]", HARDNESS[i][j]);
		}
		printf("|\n");
	}
	for(i=0;i<82;i++) printf("-");
	printf("\n");
}

void generate_dungeon(){
	int t, i, j;

	srand((t=time(NULL)));
	mvprintw(0, 0, "Seed: %d\n", t);
	clear_dungeon();

	int howManyRooms=0;
	struct room odd;
	struct room even;

	while(howManyRooms<NUM_OF_ROOMS){
		if(howManyRooms%2==0){ //EVEN
			even.x_pos = rand()%(NUMCOLS-MIN_WIDTH-1)+1; // range(1-77)
			even.y_pos = rand()%(NUMROWS-MIN_HEIGHT-1)+1; // range(1-17)
			even.width= rand()%12+3; // range(3-15)
			even.height= rand()%8+2; // range(2-12)
			even.position[dim_x]=even.x_pos;
			even.position[dim_y]=even.y_pos;
			even.size[dim_x]=even.width;
			even.size[dim_y]=even.height;

			if(canPlace(even.x_pos, even.y_pos, even.width, even.height)==TRUE){
				for(i=even.y_pos; i<even.y_pos+even.height; i++){
					for(j=even.x_pos;j<even.x_pos+even.width; j++){
						DUNGEON[i][j] = ROOM;
						HARDNESS[i][j] = 0;
						map[i][j] = ter_floor_room;
					}
				}
				if(howManyRooms!=0){
					int startY =odd.y_pos;//rand()%(odd.y_pos+odd.height);//+odd.y_pos;
					int endY = even.y_pos;//rand()%(even.y_pos+even.height);//+even.y_pos;
					int startX = odd.x_pos;//rand()%(odd.x_pos+odd.width);//+odd.x_pos;
					int endX = even.x_pos;//rand()%(even.x_pos+even.width);//+even.x_pos;

					j=startX;//MIN(startX, endX);
					i=startY;//MIN(startY, endY);
					int jmax=endX;//MAX(startX, endX);
					int imax=endY;//MAX(startY, endY);

					if(startY<endY){
						while(i<imax){
							if(DUNGEON[i][j] != ROOM){
								DUNGEON[i][j] = CORRIDOR;
								HARDNESS[i][j] = 0;
								map[i][j] = ter_floor_hall;
							}
							i++;
						}
					}
					else{
						while(i>imax){
							if(DUNGEON[i][j] != ROOM){
								DUNGEON[i][j] = CORRIDOR;
								HARDNESS[i][j] = 0;
								map[i][j] = ter_floor_hall;
							}
							 i--;
						}
					}
					if(j<jmax){
						while(j<jmax){
							if(DUNGEON[i][j] != ROOM){
								DUNGEON[i][j] = CORRIDOR;
								HARDNESS[i][j] = 0;
								map[i][j] = ter_floor_hall;
							}
							j++;
						}
					}
					else{
						while(j>jmax){
							if(DUNGEON[i][j] != ROOM){
								DUNGEON[i][j] = CORRIDOR;
								HARDNESS[i][j] = 0;
								map[i][j] = ter_floor_hall;
							}
							j--;
						}
					}
				}
				rooms[howManyRooms]=even;
				howManyRooms++;
			}
		}
		else { //ODD
			odd.x_pos = rand()%(NUMCOLS-MIN_WIDTH-1)+1; // range(1-77)
			odd.y_pos = rand()%(NUMROWS-MIN_HEIGHT-1)+1; // range(1-17)
			odd.width= rand()%12+3; // range(3-15)
			odd.height= rand()%8+2; // range(2-12)
			odd.position[dim_x]=even.x_pos;
			odd.position[dim_y]=even.y_pos;
			odd.size[dim_x]=even.width;
			odd.size[dim_y]=even.height;

			if(canPlace(odd.x_pos, odd.y_pos, odd.width, odd.height)==TRUE){
				for(i=odd.y_pos; i<odd.y_pos+odd.height; i++){
					for(j=odd.x_pos;j<odd.x_pos+odd.width; j++){
						DUNGEON[i][j] = ROOM;
						HARDNESS[i][j] = 0;
						map[i][j] = ter_floor_room;
					}
				}

				int startY =odd.y_pos;//rand()%(odd.y_pos+odd.height);//+odd.y_pos;
				int endY = even.y_pos;//rand()%(even.y_pos+even.height);//+even.y_pos;
				int startX = odd.x_pos;//rand()%(odd.x_pos+odd.width);//+odd.x_pos;
				int endX = even.x_pos;//rand()%(even.x_pos+even.width);//+even.x_pos;

				j=startX;//MIN(startX, endX);
				i=startY;//MIN(startY, endY);
				int jmax=endX;//MAX(startX, endX);
				int imax=endY;//MAX(startY, endY);

				if(startY<endY){
					while(i<imax){
						if(DUNGEON[i][j] != ROOM){
							DUNGEON[i][j] = CORRIDOR;
							HARDNESS[i][j] = 0;
							map[i][j] = ter_floor_hall;
						}
						i++;
					}
				}
				else{
					while(i>imax){
						if(DUNGEON[i][j] != ROOM){
							DUNGEON[i][j] = CORRIDOR;
							HARDNESS[i][j] = 0;
							map[i][j] = ter_floor_hall;
						}
						i--;
					}
				}
				if(j<jmax){
					while(j<jmax){
						if(DUNGEON[i][j] != ROOM){
							DUNGEON[i][j] = CORRIDOR;
							HARDNESS[i][j] = 0;
							map[i][j] = ter_floor_hall;
						}
						j++;
					}
				}
				else{
					while(j>jmax){
						if(DUNGEON[i][j] != ROOM){
							DUNGEON[i][j] = CORRIDOR;
							HARDNESS[i][j] = 0;
							map[i][j] = ter_floor_hall;
						}
						j--;
					}
				}
				rooms[howManyRooms]=odd;
				howManyRooms++;
			}
		}
	}
	num_rooms=howManyRooms;

	PC_X= rooms[0].x_pos+(rooms[0].width/2);
	PC_Y= rooms[0].y_pos+(rooms[0].height/2);

	int rand_room = rand()%(NUM_OF_ROOMS-1)+1;

	int x = rooms[rand_room].x_pos + (rand()%rooms[rand_room].width);
	int y = rooms[rand_room].y_pos + (rand()%rooms[rand_room].height);
	DUNGEON[y][x]='<';

	rand_room = rand()%(NUM_OF_ROOMS-1)+1;
	x = rooms[rand_room].x_pos + (rand()%rooms[rand_room].width);
	y = rooms[rand_room].y_pos + (rand()%rooms[rand_room].height);
	DUNGEON[y][x]='>';

	display_dungeon();
}

/*
 * Each room measures at least 3 units in the x direction and at least 2 units in the y direction.
 * Rooms need not be rectangular, but neither may they contact one another. There must be at least 1
 * cell of non-room between any two different rooms.
 */
int canPlace(int col, int row, int howLargeCol, int howLargeRow){
	//out of bounds
	if(col+howLargeCol>=NUMCOLS || row+howLargeRow>=NUMROWS) return FALSE;
	//change bounds for checking
	if(howLargeCol+col<NUMCOLS){
		howLargeCol = howLargeCol+1;
	}
	if(howLargeRow+row<NUMROWS){
		howLargeRow = howLargeRow+1;
	}
	if(col>0){
		col = col-1;
		howLargeCol = howLargeCol+1;
	}
	if(row>0){
		row = row-1;
		howLargeRow = howLargeRow+1;
	}
	int i, j;
	for(i=row; i<howLargeRow+row; i++){
		for(j=col; j<howLargeCol+col; j++){
			if(DUNGEON[i][j]==ROOM){
				return FALSE;
			}
		}
	}
	return TRUE;
}

//NOTES
///////////////////////////////////////////////////////////////////////////////////////////////////////
//Player Character and Monsters
///////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * The PC is represented by an ‘@’.
 * Add a switch, --nummon, that takes an integer count of the number of monsters to scatter around.
 * Monsters are represented by letters.
 *
 * we’re going to use letters and numbers based on monster characteristics (see below)
 * so that we can know their abilities at a glance.
 *
 * Monsters have a 50% probability of each of the following characteristics:
 *
 * • Intelligence: Intelligent monsters understand the dungeon layout and move on the shortest path (as
 * per path finding) toward the PC. Unintelligent monsters move in a straight line toward the PC (which
 * may trap them in a corner or force them to tunnel through the hardest rocks). Intelligent monsters also
 * can remember the last known position of a previously seen PC.
 *
 * • Telepathy: Telepathic monsters always know where the PC is. Non-telepathic monsters only know
 * the position of the PC if they have line of sight (smart ones can remember the position where they
 * last saw the PC when line of sight is broken). Telepathic monsters will always move toward the PC.
 * Non-telepathic monsters will only move toward a visible PC or a remembered last position.
 *
 * • Tunneling Ability: Tunneling monsters can tunnel through rock. Non-tunneling monsters cannot.
 * These two classes will use different distance maps. Tunneling monsters when attempting to move
 * through rock will subtract 85 from its hardness, down to a minimum of zero. When the hardness
 * reaches zero, the rock becomes corridor and the monster immediately moves. Reducing the hardness
 * of rock forces a tunneling distance map recalculation. Converting rock to corridor forces tunneling
 * and non-tunneling distance map recalculations.
 *
 * • Erratic Behavior: Erratic monsters have a 50% chance of moving as per their other characteristics.
 * Otherwise they move to a random neighboring cell (tunnelers may tunnel, but non-tunnelers must
 * move to an open cell).
 *
 * Since there are 4 binary characteristics, we can map the 16 possible monsters to the 16 hexadecimal
 * digits by assigning each characteristic to a bit. We’ll assign intelligence to the least significant bit
 * (on the right), telepathy next, tunneling third, and erratic behavior last (on the left), thus we have:
 *
 * Binary 					0000  0001  0010  0011  0100  0101  0110  0111
 * Decimal 					   0     1     2     3     4     5     6     7
 * Represented by 			   0     1     2     3     4     5     6     7
 * Binary (cont.) 			1000  1001  1010  1011  1100  1101  1110  1111
 * Decimal (cont.) 			   8     9    10    11    12    13    14    15
 * Represented by (cont.) 	   8     9     a     b     c     d     e     f
 *
 * So you can observe the behavior of, say, a b monster, and know that it has the intelligence, tunneling,
 * and erratic characteristics, or of a 3 and know that it is smart and telepathic.
 *
 * One more characteristic we’ll add is speed. Each monster gets a speed between 5 and 20. The PC gets
 * a speed of 10. Every character (PC and NPCs) goes in a priority queue, prioritized on the game turn of
 * their next move. Each character moves every floor(100/speed) turns (integer math, so this is just a normal
 * division). The game turn starts at zero, as do all characters’ first moves, and advances to the value at the
 * front of the priority queue every time it is dequeued.
 *
 * A system built with this kind of priority queue drive mechanism is known as a discrete event simulator.
 * It’s not actually necessary to keep track of the game turn. Since the priority queue is sorted by game turn,
 * the game turn is, by definition, the next turn of the character at the front of the queue.
 *
 * So the PC moves around like a drunken, roving idiot according to an algorithm that you devise. All
 * monsters who know where the PC is, move toward it, and all monster’s who know where the PC was,
 * move toward that location. If you like, you could make non-telepathic, smart monsters guess where to go
 * next, maybe based on an observed direction vector, and non-telepathic dumb monsters wander the corridors
 * randomly. All characters move at most one cell per move. Eventually, two characters are going to arrive at
 * the same position. In that case, the new arrival kills the original occupant. No qualms. The game ends with
 * a win if the PC is the last remaining character (unlikely, since the NPCs can be up to twice as fast as the
 * PC). It ends with a loss if the PC dies.
 *
 * Redraw the dungeon after each PC move, pause so that an observer can see the updates (experiment with sleep(3) and
 * usleep(3) to find a suitable pause), and when the game ends, print the win/lose status to the terminal before exiting
 */
/*
 * If you put a call to sleep() or usleep() between each print, it will pause between prints like frames in a movie.
 * Adjust the length of your pause to something that you find comfortable for observing and testing the behavior of your
 * characters.  Something like usleep(250000) (1/4 second) is probably good.  sleep() has 1 second precision.  usleep()
 * theoretically has 1 microsecond precision; in practice, I think the least significant decimal is garbage and you get
 * 10 microseconds of precision.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////
//PATH FINDING
///////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * To find that path, you’ll need to implement a path-finding algorithm. We’re going to have some
 * monsters that can tunnel through walls and others that can only move through open space, so
 * we’ll actually need two slightly different pathfinding algorithms. In both cases we’ll use
 * Dijkstra’s Algorithm, treating each cell in the dungeon as a cell in a graph.
 */
/*
 * For the non-tunneling monsters, we’ll give a weight of 1 for floor and ignore wall cells
 * (i.e., don’t try to find paths through walls). For the tunnelers, we’ll have to use weights
 * based on the hardness; cells with a hardness of 0 have a weight of 1, and cells with hardnesses
 * in the ranges [1; 84]; [85; 170], and [171; 254] have weights of 1, 2, and 3, respectively. A
 * hardness of 255 has infinite weight. We don’t have to assign a value to this. Instead, we
 * simply do not put it in the queue.
 */
/*
 * A na¨ıve implementation will call pathfinding for every monster in the dungeon, but in practice,
 * every monster is trying to get to the same place, so rather than calculating paths from the
 * monsters to the player character (PC), we can instead calculate the distance from the PC to
 * every point in the dungeon, and this only needs to be updated when the PC moves or the dungeon
 * changes. Each monster will choose to move to the neighboring cell with the lowest distance to
 * PC. This is gradient descent; the monsters move “downhill”. Unless the monster is already
 * collocated with the PC, there is always at least one cell with a shorter distance than its
 * current cell. In the case of multiple downhill cells having the same distance, the monster may
 * choose any one of them.
 */
/*
 * Obviously, you’ll need a priority queue, one with a decrease priority operation. You may use
 * the Fibonacci queue that I provided with my solution to 1.01. You may use the binary heap that
 * I’m posting with this assignment. Or you may implement any other priority queue you like.
 */
/*
 * BFS rooted on PC with a regular queue can also do the job. Why do we need a priority queue?
 *
 * It can do the job for open spaces, but it can't do it for walls.  You may use BFS for the open
 * spaces, but you'll need Dijkstra for the tunneling monsters.  BFS is essentially a special case
 * of Dijkstra for unweighted (or uniformly weighted) graphs.
 */
/*
 * 1  function Dijkstra(Graph, source):
 * 2      dist[source] ← 0         // Initialization
 * 3
 * 4      create vertex set Q
 * 5
 * 6      for each vertex v in Graph:
 * 7          if v ≠ source
 * 8              dist[v] ← INFINITY       // Unknown distance from source to v
 * 9              prev[v] ← UNDEFINED       // Predecessor of v
 * 10
 * 11         Q.add_with_priority(v, dist[v])
 * 12
 * 13
 * 14     while Q is not empty:                // The main loop
 * 15         u ← Q.extract_min()               // Remove and return best vertex
 * 16         for each neighbor v of u:          // only v that is still in Q
 * 17             alt = dist[u] + length(u, v)
 * 18             if alt < dist[v]
 * 19                 dist[v] ← alt
 * 20                 prev[v] ← u
 * 21                 Q.decrease_priority(v, alt)
 * 22
 * 23     return dist[], prev[]
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////
//SAVE AND LOAD
///////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * We’ll store all of our game data for our Roguelike game in a hidden directory one level
 * below our home directories. We’ll call it, creatively enough, .rlg327.
 */
/*
 * For now, our default will always be to generate a new dungeon, display it, and exit. We’ll
 * add two switches, --save and --load. The save switch will cause the game to save the dungeon
 * to disk before terminating. The load switch will load the dungeon from disk, rather than
 * generate a new one, then display it and exit. The game can take both switches at the same time,
 * in which case it reads the dungeon from disk, displays it, rewrites it, and exits. You should
 * be able to load, display, and save the same dungeon over and over. If things change from run
 * to run, you have a bug. This is a very good test of both your save and load routines!
 *
 * Even if your dungeon generation routines limit your dungeon to a fixed maximum number of rooms,
 * your load routines should have no limit, otherwise, some of us will write dungeons that others
 * cannot read.
 */
/*
 * Most of the data will be written as binary.  Only the semantic marker, RLG327, will be "text".
 * Whenever you write or read a value bigger than 1 byte to a file or network, you have to worry
 * about endianness, the order of the bytes in the value.  Intel systems are little endian,
 * meaning the least significant byte comes first.  Almost all other systems are big endian.
 * The computing world has standardized on big endian as the byte order of portable data, and
 * it's often referred to as "network order".  We'll write our files in network order so that
 * they can be read on any system.  There are standard functions to help with this.
 * See endian(3) for a bunch of them.  Some older ones are ntohl() and htonl()
 * (I'm not sure what section these are in offhand).
 *
 * getenv(3) should always return the same directory no matter where you are, so it's always safe
 * to use.
 *
 * Use "fread(charbuffer, sizeof(char), 6, ifstream)" to read "RLG327"
 *
 * Use "fread(intbuffer, sizeof(int), 2, ifstream)" to read version marker and size of the file.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////
//DUNGEON GENERATION
///////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 *• All code is in C.
 *
 *• Dungeon measures 80 units in the x (horizontal) direction and 21 units in the y (vertical)
 *	Dungeon direction. A Dungeon standard terminal is 80 × 24, and limiting the dungeon to 21
 *	rows leaves three rows for text, things like gameplay messages and player status.
 *
 *• Require at least 5 rooms per dungeon
 *
 *• Each room measures at least 3 units in the x direction and at least 2 units in the y direction.
 *
 *• Rooms need not be rectangular, but neither may they contact one another. There must be at least
 *	1 cell of non-room between any two different rooms.
 *
 *• The outermost cells of the dungeon are immutable, thus they must remain rock and cannot be part
 *	of any room or corridor.
 *
 *• Room cells should be drawn with periods, corridor cells with hashes, and rock with spaces.
 *
 *• The dungeon should be fully connected.
 *
 *• Corridors should not extend into rooms, e.g., no hashes should be rendered inside rooms.
 *
 */
