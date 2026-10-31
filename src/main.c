#include <SDL2/SDL.h>
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
#define CATALOG_URL "https://raw.githubusercontent.com/raincuh1-boop/PSVault/main/catalog.tsv"
#define UA "PSVault/0.1 (PlayStation 4)"

typedef struct { char title[96], category[48], version[24], size[24], url[768]; } Game;
typedef struct { int game, state; uint64_t got,total; char path[256],error[128]; } Item;
enum { HOME, CATALOG, DOWNLOADS, SETTINGS };
enum { QUEUED, ACTIVE, DONE, FAILED };

static Game games[MAX_GAMES];
static Item queue[MAX_QUEUE];
static int game_count=0,queue_count=0,selected=0,qselected=0,page=HOME,running=1;
static SDL_Window *win;
static SDL_Renderer *ren;
static int net_pool=0,ssl_ctx=0,http_ctx=0;

static void box(int x,int y,int w,int h,int r,int g,int b) {
 SDL_SetRenderDrawColor(ren,r,g,b,255); SDL_Rect a={x,y,w,h}; SDL_RenderFillRect(ren,&a);
}
static const unsigned char font[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};
static int gi(char c){c=(char)toupper((unsigned char)c);if(c>='A'&&c<='Z')return c-'A';if(c>='0'&&c<='9')return 26+c-'0';return -1;}
static void txt(int x,int y,const char*s,int z,int r,int g,int b){
 for(;*s;s++,x+=6*z){int k=gi(*s);if(k<0){if(*s=='.')box(x,y+6*z,z,z,r,g,b);else if(*s=='-')box(x,y+3*z,4*z,z,r,g,b);else if(*s==':'){box(x,y+2*z,z,z,r,g,b);box(x,y+5*z,z,z,r,g,b);}continue;}for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(font[k][yy]&(1<<(4-xx)))box(x+xx*z,y+yy*z,z,z,r,g,b);}
}
static void small(int x,int y,const char*s){txt(x,y,s,2,176,187,204);}
static void big(int x,int y,const char*s){txt(x,y,s,4,246,247,255);}
static void base(void){
 box(0,0,W,H,10,13,21);box(0,0,W,112,17,21,32);big(70,34,"PSVAULT");
 txt(360,48,"HOME   CATALOG   DOWNLOADS   SETTINGS",2,145,157,180);
 int x[]={60,250,500,800};box(x[page],100,145,5,178,139,255);
 box(0,H-58,W,58,17,21,32);
 small(70,H-36,"D PAD NAVIGATE");small(480,H-36,"X SELECT");small(750,H-36,"O BACK");small(1000,H-36,"TRIANGLE REFRESH");
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
static int open_http(const char*u,int*t,int*c,int*q){
 *t=sceHttpCreateTemplate(http_ctx,UA,ORBIS_HTTP_VERSION_1_1,1);if(*t<0)return -1;
 *c=sceHttpCreateConnectionWithURL(*t,u,1);if(*c<0)return -1;
 *q=sceHttpCreateRequestWithURL(*c,ORBIS_METHOD_GET,u,0);if(*q<0)return -1;
 if(sceHttpSendRequest(*q,NULL,0)<0)return -1;
 int32_t s=0;if(sceHttpGetStatusCode(*q,&s)<0||s<200||s>=300)return -1;return 0;
}
static int refresh_catalog(void){
 int t=0,c=0,q=0,ok=-1;if(open_http(CATALOG_URL,&t,&c,&q)<0)goto end;
 char buf[65536];size_t n=0;while(n<sizeof(buf)-1){int k=sceHttpReadData(q,buf+n,(unsigned int)(sizeof(buf)-1-n));if(k<0)goto end;if(!k)break;n+=(size_t)k;}buf[n]=0;
 game_count=0;char*save=0;for(char*l=strtok_r(buf,"\r\n",&save);l;l=strtok_r(NULL,"\r\n",&save)){
 if(*l=='#'||!*l)continue;Game g;memset(&g,0,sizeof(g));char*p=l,*f[5]={0};
 for(int i=0;i<5;i++){f[i]=p;if(!p)break;char*z=strchr(p,'|');if(z){*z=0;p=z+1;}else p=NULL;}
 if(f[0]&&f[1]&&f[2]&&f[3]&&f[4]&&strstr(f[4],".pkg")&&game_count<MAX_GAMES){
 snprintf(g.title,sizeof(g.title),"%s",f[0]);snprintf(g.category,sizeof(g.category),"%s",f[1]);snprintf(g.version,sizeof(g.version),"%s",f[2]);snprintf(g.size,sizeof(g.size),"%s",f[3]);snprintf(g.url,sizeof(g.url),"%s",f[4]);games[game_count++]=g;
 }}
 ok=0;
end:close_http(t,c,q);return ok;
}
static void render(void){
 base();
 if(page==HOME){
 big(80,175,"YOUR HOMEBREW STOREFRONT");small(84,250,"BROWSE AUTHORIZED PACKAGES AND MANAGE YOUR DOWNLOADS.");
 box(80,350,850,300,24,29,43);box(970,350,850,300,24,29,43);
 big(120,400,"CATALOG");small(120,480,"BROWSE PACKAGES");small(120,560,"X OPEN");
 big(1010,400,"DOWNLOADS");small(1010,480,"QUEUE AND PROGRESS");small(1010,560,"X OPEN");
 }else if(page==CATALOG){
 big(80,160,"PACKAGE CATALOG");small(80,225,"X ADD SELECTED PACKAGE TO QUEUE");
 if(!game_count){big(100,370,"CATALOG IS EMPTY");small(100,450,"ADD AUTHORIZED DIRECT PKG URLS TO THE REPOSITORY catalog.tsv");}
 for(int i=0;i<game_count;i++){int y=275+i*118;box(75,y,1770,100,i==selected?42:23,i==selected?37:30,i==selected?64:44);txt(105,y+20,games[i].title,3,245,247,255);txt(105,y+68,games[i].category,2,150,163,185);txt(1400,y+35,games[i].version,2,150,163,185);txt(1580,y+35,games[i].size,2,150,163,185);}
 }else if(page==DOWNLOADS){
 big(80,160,"DOWNLOAD MANAGER");
 if(!queue_count){big(100,370,"QUEUE IS EMPTY");small(100,450,"ADD A PACKAGE FROM THE CATALOG FIRST");}
 for(int i=0;i<queue_count;i++){Item*d=&queue[i];int y=250+i*145;box(80,y,1760,120,i==qselected?42:23,i==qselected?37:30,i==qselected?64:44);txt(110,y+18,games[d->game].title,3,245,247,255);const char*s=d->state==QUEUED?"QUEUED":d->state==ACTIVE?"DOWNLOADING":d->state==DONE?"DOWNLOADED":"FAILED";txt(110,y+72,s,2,170,180,200);box(500,y+78,1300,12,52,57,70);if(d->total)box(500,y+78,(int)((d->got*1300)/d->total),12,178,139,255);}
 small(80,900,"X START OR RETRY   SQUARE REMOVE   START PROCESS QUEUE");
 }else{
 big(80,160,"SETTINGS");small(100,280,"DOWNLOAD FOLDER: /data/PSVault/");small(100,360,"CATALOG: PSVAULT GITHUB REPOSITORY / catalog.tsv");small(100,460,"DOWNLOADS ARE SAVED AS PKG FILES FOR PACKAGE INSTALLER.");small(100,530,"INSTALLATION IS MANUAL THROUGH YOUR JAILBROKEN PS4 PACKAGE INSTALLER.");small(100,620,"NO LICENSE BYPASS OR UNAUTHORIZED COMMERCIAL GAME DISTRIBUTION.");
 }
 SDL_RenderPresent(ren);
}
static void queue_add(int i){
 if(i<0||i>=game_count||queue_count>=MAX_QUEUE)return;
 for(int n=0;n<queue_count;n++)if(queue[n].game==i&&queue[n].state!=FAILED)return;
 Item*d=&queue[queue_count++];memset(d,0,sizeof(*d));d->game=i;d->state=QUEUED;
 snprintf(d->path,sizeof(d->path),"/data/PSVault/%s.pkg",games[i].title);
 for(char*p=d->path+strlen("/data/PSVault/");*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')*p='_';
}
static void progress(Item*d){
 base();big(80,190,"DOWNLOADING");big(80,290,games[d->game].title);box(100,470,1650,24,48,54,68);
 if(d->total)box(100,470,(int)(d->got*1650/d->total),24,178,139,255);
 char s[100];snprintf(s,sizeof(s),"%llu / %llu BYTES",(unsigned long long)d->got,(unsigned long long)d->total);small(100,540,s);small(100,640,"KEEP PSVAULT OPEN");
 SDL_RenderPresent(ren);
}
static int download_item(Item*d){
 int t=0,c=0,q=0,ok=-1;mkdir("/data/PSVault",0777);d->state=ACTIVE;d->got=d->total=0;
 if(open_http(games[d->game].url,&t,&c,&q)<0){snprintf(d->error,sizeof(d->error),"HTTP request failed");goto out;}
 int type=0;uint64_t len=0;if(sceHttpGetResponseContentLength(q,&type,&len)==0&&type==ORBIS_HTTP_CONTENTLEN_EXIST)d->total=len;
 FILE*f=fopen(d->path,"wb");if(!f){snprintf(d->error,sizeof(d->error),"Cannot write to download folder");goto out;}
 unsigned char b[65536];for(;;){int n=sceHttpReadData(q,b,sizeof(b));if(n<0){snprintf(d->error,sizeof(d->error),"Connection interrupted");break;}if(!n){ok=0;break;}if(fwrite(b,1,(size_t)n,f)!=(size_t)n){snprintf(d->error,sizeof(d->error),"Disk write failed");break;}d->got+=(uint64_t)n;progress(d);}
 fclose(f);if(ok<0)remove(d->path);
out:close_http(t,c,q);d->state=ok==0?DONE:FAILED;return ok;
}
static void start_queue(void){for(int i=0;i<queue_count&&running;i++)if(queue[i].state==QUEUED||queue[i].state==FAILED)download_item(&queue[i]);}
static void remove_queue(void){if(!queue_count)return;for(int i=qselected;i<queue_count-1;i++)queue[i]=queue[i+1];queue_count--;if(qselected>=queue_count)qselected=queue_count?queue_count-1:0;}
static void key(SDL_Keycode k){
 if(k==SDLK_ESCAPE||k==SDLK_BACKSPACE){if(page!=HOME)page=HOME;else running=0;}
 if(k==SDLK_LEFT&&page>0)page--;if(k==SDLK_RIGHT&&page<3)page++;
 if(k==SDLK_UP){if(page==CATALOG&&selected>0)selected--;if(page==DOWNLOADS&&qselected>0)qselected--;}
 if(k==SDLK_DOWN){if(page==CATALOG&&selected<game_count-1)selected++;if(page==DOWNLOADS&&qselected<queue_count-1)qselected++;}
 if(k==SDLK_RETURN||k==SDLK_SPACE){if(page==HOME)page=CATALOG;else if(page==CATALOG)queue_add(selected);else if(page==DOWNLOADS&&queue_count)download_item(&queue[qselected]);}
 if(k==SDLK_r&&page==CATALOG){refresh_catalog();selected=0;}
 if(k==SDLK_x&&page==DOWNLOADS&&queue_count)download_item(&queue[qselected]);
 if(k==SDLK_DELETE&&page==DOWNLOADS)remove_queue();
 if(k==SDLK_s&&page==DOWNLOADS)start_queue();
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0)return 1;
 win=SDL_CreateWindow("PSVault",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_FULLSCREEN);
 if(!win)win=SDL_CreateWindow("PSVault",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,0);
 ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!ren)ren=SDL_CreateRenderer(win,-1,0);if(!ren)return 2;
 mkdir("/data/PSVault",0777);if(net_init()==0)refresh_catalog();
 while(running){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)running=0;if(e.type==SDL_KEYDOWN)key(e.key.keysym.sym);if(e.type==SDL_CONTROLLERBUTTONDOWN){switch(e.cbutton.button){case SDL_CONTROLLER_BUTTON_DPAD_UP:key(SDLK_UP);break;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:key(SDLK_DOWN);break;case SDL_CONTROLLER_BUTTON_DPAD_LEFT:key(SDLK_LEFT);break;case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:key(SDLK_RIGHT);break;case SDL_CONTROLLER_BUTTON_A:key(SDLK_RETURN);break;case SDL_CONTROLLER_BUTTON_B:key(SDLK_ESCAPE);break;case SDL_CONTROLLER_BUTTON_X:key(SDLK_DELETE);break;case SDL_CONTROLLER_BUTTON_Y:key(SDLK_r);break;case SDL_CONTROLLER_BUTTON_BACK:key(SDLK_BACKSPACE);break;case SDL_CONTROLLER_BUTTON_START:key(SDLK_s);break;}}}
 render();SDL_Delay(8);}
 if(http_ctx>0)sceHttpTerm(http_ctx);if(ssl_ctx>0)sceSslTerm(ssl_ctx);if(net_pool>0)sceNetPoolDestroy(net_pool);
 SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
