#include <SDL2/SDL.h>
#include <stdbool.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define W 1920
#define H 1080
#define MAX_GAMES 128
#define MAX_QUEUE 32
#define MAX_CATEGORIES 32
#define CATALOG_URL "https://raw.githubusercontent.com/raincuh1-boop/PSVault/main/catalog.tsv"
#define UA "PSVault/0.2 (PlayStation 4)"
#define DATA_DIR "/data/PSVault"
#define QUEUE_FILE DATA_DIR "/downloads.tsv"

typedef struct {
 char title[96], category[48], version[24], size[24], url[768], description[256];
} Game;
typedef struct {
 int game, state;
 uint64_t got, total;
 uint32_t started_ms;
 double speed;
 char path[256], error[128];
} Item;
enum { HOME, CATALOG, DOWNLOADS, SETTINGS, DETAILS };
enum { QUEUED, ACTIVE, DONE, FAILED, PAUSED };

static Game games[MAX_GAMES];
static Item queue[MAX_QUEUE];
static char categories[MAX_CATEGORIES][48];
static int game_count=0;
static int category_count=0, category_filter=-1;
static int filtered[MAX_GAMES], filtered_count=0;
static int queue_count=0, selected=0, qselected=0, detail_game=-1;
static int page=HOME, running=1, search_open=0, keyboard_cursor=0, download_cancel_requested=0;
static char search_text[64]="";
static SDL_Window *win;
static SDL_Renderer *ren;
static int net_pool=0, ssl_ctx=0, http_ctx=0;

static void box(int x,int y,int w,int h,int r,int g,int b) {
 SDL_SetRenderDrawColor(ren,r,g,b,255); SDL_Rect a={x,y,w,h}; SDL_RenderFillRect(ren,&a);
}
static const unsigned char font[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};
static int gi(char c){c=(char)toupper((unsigned char)c);if(c>='A'&&c<='Z')return c-'A';if(c>='0'&&c<='9')return 26+c-'0';return -1;}
static void txt(int x,int y,const char*s,int z,int r,int g,int b){
 for(;*s;s++,x+=6*z){int k=gi(*s);if(k<0){if(*s=='.')box(x,y+6*z,z,z,r,g,b);else if(*s=='-')box(x,y+3*z,4*z,z,r,g,b);else if(*s==':'){box(x,y+2*z,z,z,r,g,b);box(x,y+5*z,z,z,r,g,b);}else if(*s=='_')box(x,y+7*z,4*z,z,r,g,b);else if(*s=='/')box(x+2*z,y,z,z,r,g,b);continue;}for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(font[k][yy]&(1<<(4-xx)))box(x+xx*z,y+yy*z,z,z,r,g,b);}
}
static void small(int x,int y,const char*s){txt(x,y,s,2,176,187,204);}
static void big(int x,int y,const char*s){txt(x,y,s,4,246,247,255);}
static int contains_ci(const char *hay,const char *needle){
 if(!needle[0])return 1;
 for(;*hay;hay++){size_t i=0;while(needle[i]&&hay[i]&&tolower((unsigned char)hay[i])==tolower((unsigned char)needle[i]))i++;if(!needle[i])return 1;}
 return 0;
}
static void build_categories(void){
 category_count=0;
 for(int i=0;i<game_count;i++){
  int found=0;for(int j=0;j<category_count;j++)if(!strcmp(categories[j],games[i].category)){found=1;break;}
  if(!found&&category_count<MAX_CATEGORIES)snprintf(categories[category_count++],48,"%s",games[i].category);
 }
 if(category_filter>=category_count)category_filter=-1;
}
static void build_filtered(void){
 filtered_count=0;
 for(int i=0;i<game_count;i++){
  if(category_filter>=0&&strcmp(games[i].category,categories[category_filter]))continue;
  if(!contains_ci(games[i].title,search_text)&&!contains_ci(games[i].category,search_text)&&!contains_ci(games[i].description,search_text))continue;
  filtered[filtered_count++]=i;
 }
 if(selected>=filtered_count)selected=filtered_count?filtered_count-1:0;
}
static void base(void){
 box(0,0,W,H,10,13,21);box(0,0,W,112,17,21,32);big(70,34,"PSVAULT");
 txt(360,48,"HOME   CATALOG   DOWNLOADS   SETTINGS",2,145,157,180);
 int x[]={60,250,500,800};int p=(page==DETAILS)?CATALOG:page;box(x[p],100,145,5,178,139,255);
 box(0,H-58,W,58,17,21,32);
 small(70,H-36,"D PAD NAVIGATE");small(390,H-36,"CROSS SELECT");small(690,H-36,"CIRCLE BACK");
 small(990,H-36,"TRIANGLE REFRESH");small(1390,H-36,"START DOWNLOAD QUEUE");
}
static int net_init(void){
 if(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET)<0)return -1;
 if(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP)<0)return -1;
 if(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL)<0)return -1;
 sceNetInit();net_pool=sceNetPoolCreate("psvault",1024*1024,0);if(net_pool<0)return -1;
 ssl_ctx=sceSslInit(SSL_POOLSIZE);if(ssl_ctx<0)return -1;
 http_ctx=sceHttpInit(net_pool,ssl_ctx,LIBHTTP_POOLSIZE);return http_ctx<0?-1:0;
}
static void close_http(int t,int c,int q){if(q>0)sceHttpDeleteRequest(q);if(c>0)sceHttpDeleteConnection(c);if(t>0)sceHttpDeleteTemplate(t);}
static int open_http(const char*u,int*t,int*c,int*q,uint64_t offset,int32_t *status_out){
 *t=sceHttpCreateTemplate(http_ctx,UA,ORBIS_HTTP_VERSION_1_1,1);if(*t<0)return -1;
 *c=sceHttpCreateConnectionWithURL(*t,u,1);if(*c<0)return -1;
 *q=sceHttpCreateRequestWithURL(*c,ORBIS_METHOD_GET,u,0);if(*q<0)return -1;
 if(sceHttpAddRequestHeader(*q,"Accept-Encoding","identity",0)<0)return -1;
 if(offset>0){char range[64];snprintf(range,sizeof(range),"bytes=%llu-",(unsigned long long)offset);if(sceHttpAddRequestHeader(*q,"Range",range,0)<0)return -1;}
 if(sceHttpSendRequest(*q,NULL,0)<0)return -1;
 int32_t s=0;if(sceHttpGetStatusCode(*q,&s)<0||s<200||s>=300)return -1;
 if(status_out)*status_out=s;return 0;
}
static void parse_catalog(char *buf){
 game_count=0;char*save=0;
 for(char*l=strtok_r(buf,"\r\n",&save);l;l=strtok_r(NULL,"\r\n",&save)){
  if(*l=='#'||!*l)continue;
  char*p=l,*f[6]={0};for(int i=0;i<6;i++){f[i]=p;if(!p)break;char*z=strchr(p,'|');if(z){*z=0;p=z+1;}else p=NULL;}
  if(f[0]&&f[1]&&f[2]&&f[3]&&f[4]&&!strncmp(f[4],"https://",8)&&strstr(f[4],".pkg")&&game_count<MAX_GAMES){
   Game*g=&games[game_count++];memset(g,0,sizeof(*g));
   snprintf(g->title,sizeof(g->title),"%s",f[0]);snprintf(g->category,sizeof(g->category),"%s",f[1]);
   snprintf(g->version,sizeof(g->version),"%s",f[2]);snprintf(g->size,sizeof(g->size),"%s",f[3]);
   snprintf(g->url,sizeof(g->url),"%s",f[4]);if(f[5])snprintf(g->description,sizeof(g->description),"%s",f[5]);
  }
 }
 build_categories();build_filtered();
}
static int refresh_catalog(void){
 int t=0,c=0,q=0,ok=-1;char buf[65536];size_t n=0;
 if(http_ctx>0&&open_http(CATALOG_URL,&t,&c,&q,0,NULL)==0){
  while(n<sizeof(buf)-1){int k=sceHttpReadData(q,buf+n,(unsigned int)(sizeof(buf)-1-n));if(k<0)break;if(!k){ok=0;break;}n+=(size_t)k;}
  close_http(t,c,q);t=c=q=0;
  if(ok==0){buf[n]=0;FILE*f=fopen(DATA_DIR "/catalog.tsv","wb");if(f){fwrite(buf,1,n,f);fclose(f);}parse_catalog(buf);return 0;}
 }
 close_http(t,c,q);
 FILE*f=fopen(DATA_DIR "/catalog.tsv","rb");if(!f)return -1;
 n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;parse_catalog(buf);return 0;
}
static void make_path(Item*d){
 snprintf(d->path,sizeof(d->path),DATA_DIR "/%s.pkg",games[d->game].title);
 for(char*p=d->path+strlen(DATA_DIR "/");*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')*p='_';
}
static void save_queue(void){
 FILE*f=fopen(QUEUE_FILE,"wb");if(!f)return;
 for(int i=0;i<queue_count;i++){Item*d=&queue[i];if(d->game<0||d->game>=game_count)continue;
  fprintf(f,"%d|%llu|%llu|%s\n",d->state,(unsigned long long)d->got,(unsigned long long)d->total,games[d->game].url);
 }
 fclose(f);
}
static void part_path(const Item*d,char*out,size_t n){snprintf(out,n,"%s.part",d->path);}
static void load_queue(void){
 FILE*f=fopen(QUEUE_FILE,"rb");if(!f)return;char line[1200];queue_count=0;
 while(fgets(line,sizeof(line),f)&&queue_count<MAX_QUEUE){
  char*save=0;char*a=strtok_r(line,"|\r\n",&save);char*b=strtok_r(NULL,"|\r\n",&save);
  char*c=strtok_r(NULL,"|\r\n",&save);char*u=strtok_r(NULL,"|\r\n",&save);if(!a||!b||!c||!u)continue;
  int game=-1;for(int i=0;i<game_count;i++)if(!strcmp(games[i].url,u)){game=i;break;}if(game<0)continue;
  Item*d=&queue[queue_count++];memset(d,0,sizeof(*d));d->game=game;d->state=atoi(a);d->got=(uint64_t)strtoull(b,NULL,10);d->total=(uint64_t)strtoull(c,NULL,10);
  if(d->state<QUEUED||d->state>PAUSED)d->state=FAILED;else if(d->state==ACTIVE)d->state=PAUSED;
  make_path(d);char partial[320];part_path(d,partial,sizeof(partial));struct stat st;
  if(d->state==DONE){if(stat(d->path,&st)!=0)d->state=QUEUED;}
  else if(stat(partial,&st)==0&&st.st_size>0&&(d->total==0||(uint64_t)st.st_size<=d->total)){d->got=(uint64_t)st.st_size;d->state=FAILED;}
  else {d->got=0;if(d->state!=QUEUED)d->state=FAILED;}
 }
 fclose(f);if(qselected>=queue_count)qselected=queue_count?queue_count-1:0;
}
static void queue_add(int i){
 if(i<0||i>=game_count||queue_count>=MAX_QUEUE)return;
 for(int n=0;n<queue_count;n++)if(!strcmp(games[queue[n].game].url,games[i].url)){if(queue[n].state==FAILED){queue[n].state=QUEUED;queue[n].got=queue[n].total=0;save_queue();}return;}
 Item*d=&queue[queue_count++];memset(d,0,sizeof(*d));d->game=i;d->state=QUEUED;make_path(d);save_queue();
}
static void queue_remove(void){
 if(!queue_count)return;
 for(int i=qselected;i<queue_count-1;i++)queue[i]=queue[i+1];
 queue_count--;if(qselected>=queue_count)qselected=queue_count?queue_count-1:0;save_queue();
}
static void render_keyboard(void){
 box(420,610,1080,350,25,31,47);small(465,635,"SEARCH KEYBOARD - CROSS TYPE - CIRCLE CLOSE");
 const char*keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"; 
 for(int i=0;i<40;i++){int x=470+(i%10)*96,y=680+(i/10)*54;box(x,y,82,42,i==keyboard_cursor?92:42,i==keyboard_cursor?68:50,i==keyboard_cursor?126:67);
  char s[2];s[0]=(i<38)?keys[i]:(i==38?'<':'C');s[1]=0;txt(x+27,y+12,s,2,246,247,255);
 }
 box(1380,900,100,42,70,51,100);small(1402,914,"DONE");
}
static void render(void){
 build_filtered();base();
 if(page==HOME){
  big(80,175,"YOUR HOMEBREW STOREFRONT");small(84,250,"PACKAGES, CATEGORIES AND DOWNLOADS IN ONE PLACE");
  box(80,350,850,300,24,29,43);box(970,350,850,300,24,29,43);
  big(120,400,"CATALOG");small(120,480,"BROWSE AND SEARCH PACKAGES");small(120,560,"CROSS OPEN");
  big(1010,400,"DOWNLOADS");small(1010,480,"QUEUE AND DOWNLOAD HISTORY");small(1010,560,"CROSS OPEN");
  char s[96];snprintf(s,sizeof(s),"%d PACKAGES AVAILABLE",game_count);big(85,760,s);
  snprintf(s,sizeof(s),"%d ITEMS IN DOWNLOADS",queue_count);big(85,825,s);
 }else if(page==CATALOG){
  big(80,150,"PACKAGE CATALOG");
  box(80,218,1760,64,25,31,47);small(105,242,"SEARCH:");txt(255,238,search_text,2,245,247,255);
  char filter[80];snprintf(filter,sizeof(filter),"CATEGORY: %s",category_filter<0?"ALL":categories[category_filter]);small(1100,242,filter);
  small(80,300,"CROSS DETAILS   SQUARE SEARCH   L1/R1 CATEGORY   TRIANGLE REFRESH");
  if(!filtered_count){big(100,440,game_count?"NO MATCHES":"CATALOG IS EMPTY");small(100,510,game_count?"CLEAR SEARCH OR CHANGE CATEGORY":"ADD AUTHORIZED DIRECT HTTPS PKG URLS TO catalog.tsv");}
  int start=(selected/5)*5;
  for(int n=start;n<filtered_count&&n<start+5;n++){int i=filtered[n],y=350+(n-start)*118;box(75,y,1770,100,n==selected?42:23,n==selected?37:30,n==selected?64:44);
   txt(105,y+20,games[i].title,3,245,247,255);txt(105,y+68,games[i].category,2,150,163,185);
   txt(1400,y+35,games[i].version,2,150,163,185);txt(1580,y+35,games[i].size,2,150,163,185);
  }
  if(search_open)render_keyboard();
 }else if(page==DETAILS){
  int i=detail_game;
  if(i>=0&&i<game_count){big(85,160,"PACKAGE DETAILS");box(80,245,1760,560,24,29,43);
   big(125,300,games[i].title);small(130,390,"CATEGORY");txt(390,390,games[i].category,2,245,247,255);
   small(130,450,"VERSION");txt(390,450,games[i].version,2,245,247,255);
   small(130,510,"PACKAGE SIZE");txt(390,510,games[i].size,2,245,247,255);
   small(130,590,"DESCRIPTION");txt(390,590,games[i].description[0]?games[i].description:"NO DESCRIPTION PROVIDED",2,190,200,216);
   small(130,680,"SOURCE");txt(390,680,"DIRECT HTTPS PKG URL",2,190,200,216);
   box(85,850,400,80,73,52,111);big(125,874,"ADD TO QUEUE");
   small(540,880,"CROSS ADD   CIRCLE BACK");
  }
 }else if(page==DOWNLOADS){
  big(80,150,"DOWNLOAD MANAGER");small(80,220,"QUEUED, DOWNLOADED AND FAILED ITEMS ARE SAVED BETWEEN SESSIONS");
  if(!queue_count){big(100,420,"YOUR DOWNLOADS ARE EMPTY");small(100,500,"ADD A PACKAGE FROM THE CATALOG");}
  int start=(qselected/5)*5;
  for(int n=start;n<queue_count&&n<start+5;n++){Item*d=&queue[n];int y=285+(n-start)*120;
   box(80,y,1760,102,n==qselected?42:23,n==qselected?37:30,n==qselected?64:44);
   if(d->game>=0&&d->game<game_count)txt(110,y+15,games[d->game].title,3,245,247,255);
   const char*s=d->state==QUEUED?"QUEUED":d->state==ACTIVE?"DOWNLOADING":d->state==DONE?"DOWNLOADED":d->state==PAUSED?"PAUSED":"FAILED";
   txt(110,y+63,s,2,170,180,200);
   box(650,y+70,1050,10,52,57,70);if(d->total)box(650,y+70,(int)((d->got*1050)/d->total),10,178,139,255);
  }
  small(80,920,"CROSS START / RETRY   SQUARE REMOVE FROM HISTORY   START PROCESS QUEUE");
 }else{
  big(80,160,"SETTINGS");box(80,245,1760,530,24,29,43);
  small(120,300,"DOWNLOAD FOLDER: " DATA_DIR);small(120,380,"CATALOG SOURCE: PSVAULT GITHUB REPOSITORY / catalog.tsv");
  small(120,470,"DOWNLOADS ARE STORED LOCALLY AS PKG FILES.");
  small(120,540,"USE THE PS4 PACKAGE INSTALLER TO INSTALL DOWNLOADED PACKAGES.");
  small(120,640,"DOWNLOADS REQUIRE A DIRECT HTTPS .PKG URL.");
  small(120,710,"ONLY DISTRIBUTE HOMEBREW OR PACKAGES YOU ARE AUTHORIZED TO SHARE.");
 }
 SDL_RenderPresent(ren);
}
static void update_progress(Item*d){
 uint32_t elapsed=SDL_GetTicks()-d->started_ms;if(elapsed<1)elapsed=1;
 d->speed=(double)d->got*1000.0/(double)elapsed;
 base();big(80,175,"DOWNLOADING PACKAGE");if(d->game>=0&&d->game<game_count)big(80,260,games[d->game].title);
 box(100,440,1650,28,48,54,68);if(d->total){uint64_t pc=d->got>=d->total?1650:(d->got*1650)/d->total;box(100,440,(int)pc,28,178,139,255);}
 char s[160];snprintf(s,sizeof(s),"%llu / %llu BYTES",(unsigned long long)d->got,(unsigned long long)d->total);small(100,520,s);
 snprintf(s,sizeof(s),"SPEED %.2f MB/S",d->speed/1048576.0);small(100,590,s);
 if(d->total&&d->speed>1){uint64_t left=(uint64_t)((double)(d->total-d->got)/d->speed);snprintf(s,sizeof(s),"TIME LEFT %llu MIN %llu SEC",(unsigned long long)(left/60),(unsigned long long)(left%60));small(100,660,s);}
 small(100,760,"CIRCLE TO PAUSE - PARTIAL FILE WILL BE KEPT FOR RESUME");SDL_RenderPresent(ren);
}
static void poll_download_controls(void){
 SDL_Event e;while(SDL_PollEvent(&e)){
  if(e.type==SDL_QUIT){running=0;download_cancel_requested=1;}
  else if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE)download_cancel_requested=1;
  else if(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.button==SDL_CONTROLLER_BUTTON_B)download_cancel_requested=1;
 }
}
static int download_item(Item*d){
 int t=0,c=0,q=0,ok=-1,status=0,type=0;uint64_t len=0,offset=0,checkpoint=0;
 char partial[320];mkdir(DATA_DIR,0777);make_path(d);part_path(d,partial,sizeof(partial));
 struct stat st;if(stat(partial,&st)==0&&st.st_size>0)offset=(uint64_t)st.st_size;
 if(d->total&&offset>d->total){remove(partial);offset=0;}
 if(d->total&&offset==d->total){if(rename(partial,d->path)==0){d->got=d->total;d->state=DONE;save_queue();return 0;}remove(partial);offset=0;}
 d->state=ACTIVE;d->got=offset;d->started_ms=SDL_GetTicks();d->speed=0;d->error[0]=0;download_cancel_requested=0;save_queue();
 if(open_http(games[d->game].url,&t,&c,&q,offset,&status)<0){snprintf(d->error,sizeof(d->error),"HTTP REQUEST FAILED");goto out;}
 if(offset>0&&status==200){
  close_http(t,c,q);t=c=q=0;offset=0;d->got=0;d->total=0;
  if(open_http(games[d->game].url,&t,&c,&q,0,&status)<0){snprintf(d->error,sizeof(d->error),"HTTP RESTART REQUEST FAILED");goto out;}
 }
 if(offset>0&&status!=206){snprintf(d->error,sizeof(d->error),"SERVER DOES NOT SUPPORT RANGE RESUME");goto out;}
 if(sceHttpGetResponseContentLength(q,&type,&len)==0&&type==ORBIS_HTTP_CONTENTLEN_EXIST){
  if(status==206)d->total=offset+len;else d->total=len;
 }
 FILE*f=fopen(partial,offset>0?"ab":"wb");if(!f){snprintf(d->error,sizeof(d->error),"CANNOT WRITE TO DOWNLOAD FOLDER");goto out;}
 checkpoint=d->got+2*1024*1024;
 unsigned char b[65536];
 for(;;){
  poll_download_controls();if(download_cancel_requested){snprintf(d->error,sizeof(d->error),"PAUSED BY USER");break;}
  int n=sceHttpReadData(q,b,sizeof(b));if(n<0){snprintf(d->error,sizeof(d->error),"CONNECTION INTERRUPTED");break;}if(!n){ok=0;break;}
  if(fwrite(b,1,(size_t)n,f)!=(size_t)n){snprintf(d->error,sizeof(d->error),"DISK WRITE FAILED");break;}
  d->got+=(uint64_t)n;update_progress(d);
  if(d->got>=checkpoint){fflush(f);save_queue();checkpoint=d->got+2*1024*1024;}
 }
 fflush(f);fclose(f);
 if(ok==0&&d->total&&d->got!=d->total){ok=-1;snprintf(d->error,sizeof(d->error),"PACKAGE SIZE MISMATCH");}
 if(ok==0){if(rename(partial,d->path)!=0){ok=-1;snprintf(d->error,sizeof(d->error),"COULD NOT FINALIZE PACKAGE FILE");}}
out:close_http(t,c,q);d->state=ok==0?DONE:(download_cancel_requested?PAUSED:FAILED);
 if(ok<0){struct stat ps;if(stat(partial,&ps)==0)d->got=(uint64_t)ps.st_size;}
 save_queue();return ok;
}
static void start_queue(void){for(int i=0;i<queue_count&&running;i++)if(queue[i].state==QUEUED||queue[i].state==FAILED||queue[i].state==PAUSED){download_item(&queue[i]);qselected=i;if(download_cancel_requested)break;}}
static void search_append(char c){size_t n=strlen(search_text);if(n<sizeof(search_text)-1){search_text[n]=c;search_text[n+1]=0;selected=0;build_filtered();}}
static void search_delete(void){size_t n=strlen(search_text);if(n){search_text[n-1]=0;selected=0;build_filtered();}}
static void keyboard_activate(void){
 const char*keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
 if(keyboard_cursor<38)search_append(keys[keyboard_cursor]);else if(keyboard_cursor==38)search_delete();else if(keyboard_cursor==39){search_text[0]=0;selected=0;build_filtered();}else search_open=0;
}
static void key(SDL_Keycode k){
 if(page==CATALOG&&search_open){
  if(k==SDLK_ESCAPE||k==SDLK_BACKSPACE){search_open=0;return;}
  if(k==SDLK_LEFT&&keyboard_cursor%10>0)keyboard_cursor--;
  if(k==SDLK_RIGHT&&keyboard_cursor%10<9)keyboard_cursor++;
  if(k==SDLK_UP&&keyboard_cursor>=10)keyboard_cursor-=10;
  if(k==SDLK_DOWN&&keyboard_cursor<30)keyboard_cursor+=10;
  if(k==SDLK_RETURN||k==SDLK_SPACE)keyboard_activate();
  if(k==SDLK_DELETE)search_delete();
  return;
 }
 if(k==SDLK_ESCAPE||k==SDLK_BACKSPACE){if(page==DETAILS)page=CATALOG;else if(page!=HOME)page=HOME;else running=0;return;}
 if(page==CATALOG&&k==SDLK_x){search_open=1;keyboard_cursor=0;return;}
 if(k==SDLK_LEFT&&page>0&&page!=DETAILS)page--;
 if(k==SDLK_RIGHT&&page<3&&page!=DETAILS)page++;
 if(k==SDLK_UP){if(page==CATALOG&&selected>0)selected--;if(page==DOWNLOADS&&qselected>0)qselected--;}
 if(k==SDLK_DOWN){if(page==CATALOG&&selected<filtered_count-1)selected++;if(page==DOWNLOADS&&qselected<queue_count-1)qselected++;}
 if(k==SDLK_RETURN||k==SDLK_SPACE){
  if(page==HOME)page=CATALOG;
  else if(page==CATALOG&&filtered_count){detail_game=filtered[selected];page=DETAILS;}
  else if(page==DETAILS)queue_add(detail_game);
  else if(page==DOWNLOADS&&queue_count)download_item(&queue[qselected]);
 }
 if(k==SDLK_r&&page==CATALOG){refresh_catalog();selected=0;}
 if(k==SDLK_x&&page==DOWNLOADS&&queue_count)download_item(&queue[qselected]);
 if(k==SDLK_DELETE&&page==DOWNLOADS)queue_remove();
 if(k==SDLK_s&&page==DOWNLOADS)start_queue();
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0)return 1;
 win=SDL_CreateWindow("PSVault",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_FULLSCREEN);
 if(!win)win=SDL_CreateWindow("PSVault",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,0);
 ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!ren)ren=SDL_CreateRenderer(win,-1,0);if(!ren)return 2;
 mkdir(DATA_DIR,0777);if(net_init()==0)refresh_catalog();else refresh_catalog();load_queue();
 while(running){
  SDL_Event e;while(SDL_PollEvent(&e)){
   if(e.type==SDL_QUIT)running=0;
   if(e.type==SDL_TEXTINPUT&&page==CATALOG&&search_open){for(char*p=e.text.text;*p;p++)if(isalnum((unsigned char)*p)||*p=='-'||*p=='_')search_append(*p);}
   if(e.type==SDL_KEYDOWN)key(e.key.keysym.sym);
   if(e.type==SDL_CONTROLLERBUTTONDOWN){
    switch(e.cbutton.button){
     case SDL_CONTROLLER_BUTTON_DPAD_UP: if(search_open&&page==CATALOG)key(SDLK_UP);else key(SDLK_UP);break;
     case SDL_CONTROLLER_BUTTON_DPAD_DOWN:key(SDLK_DOWN);break;
     case SDL_CONTROLLER_BUTTON_DPAD_LEFT:key(SDLK_LEFT);break;
     case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:key(SDLK_RIGHT);break;
     case SDL_CONTROLLER_BUTTON_A:key(SDLK_RETURN);break;
     case SDL_CONTROLLER_BUTTON_B:key(SDLK_ESCAPE);break;
     case SDL_CONTROLLER_BUTTON_X:if(page==CATALOG)key(SDLK_x);else if(page==DOWNLOADS)key(SDLK_DELETE);break;
     case SDL_CONTROLLER_BUTTON_Y:if(search_open){search_text[0]=0;selected=0;build_filtered();}else if(page==CATALOG)key(SDLK_r);break;
     case SDL_CONTROLLER_BUTTON_BACK:key(SDLK_BACKSPACE);break;
     case SDL_CONTROLLER_BUTTON_START:if(page==DOWNLOADS)key(SDLK_s);else if(search_open)search_open=0;break;
     case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:if(page==CATALOG&&category_filter> -1){category_filter--;selected=0;}break;
     case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:if(page==CATALOG){category_filter++;if(category_filter>=category_count)category_filter=-1;selected=0;}break;
    }
   }
  }
  render();SDL_Delay(8);
 }
 save_queue();if(http_ctx>0)sceHttpTerm(http_ctx);if(ssl_ctx>0)sceSslTerm(ssl_ctx);if(net_pool>0)sceNetPoolDestroy(net_pool);
 SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
