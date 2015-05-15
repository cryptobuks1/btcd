//
//  db777.c
//  crypto777
//
//  Created by James on 4/9/15.
//  Copyright (c) 2015 jl777. All rights reserved.
//

#ifdef DEFINES_ONLY
#ifndef crypto777_db777_h
#define crypto777_db777_h
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sophia.h"
#include "cJSON.h"
#include "system777.c"
#include "coins777.c"
#include "storage.c"

#define DB777_RAM 1
#define DB777_HDD 2
#define DB777_NANO 4
#define DB777_FLUSH 8
#define DB777_KEY32 0x20
#define ENV777_BACKUP 0x40
#define DB777_MULTITHREAD 0x80


#define SOPHIA_USERDIR "/user"
void *db777_get(struct db777_entry **entryp,int32_t *lenp,void *transactions,struct db777 *DB,void *key,int32_t keylen);
int32_t db777_set(int32_t flags,void *transactions,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t valuelen);

uint64_t db777_ctlinfo64(void *ctl,char *field);
int32_t db777_add(int32_t forceflag,void *transactions,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t len);
int32_t db777_delete(int32_t flags,void *transactions,struct db777 *DB,void *key,int32_t keylen);
int32_t db777_sync(struct env777 *DBs,int32_t fullbackup);
void db777_free(struct db777 *DB);

int32_t db777_addstr(struct db777 *DB,char *key,char *value);
int32_t db777_findstr(char *retbuf,int32_t max,struct db777 *DB,char *key);

int32_t db777_close(struct db777 *DB);
void **db777_copy_all(int32_t *nump,struct db777 *DB,char *field,int32_t size);
struct db777 *db777_create(char *specialpath,char *subdir,char *name,char *compression,int32_t restoreflag);

int32_t env777_start(int32_t dispflag,struct env777 *DBs);
char **db777_index(int32_t *nump,struct db777 *DB,int32_t max);
int32_t db777_dump(struct db777 *DB,int32_t binarykey,int32_t binaryvalue);

extern struct db777_info SOPHIA;
extern struct db777 *DB_msigs,*DB_NXTaccts,*DB_nodestats,*DB_busdata;//,*DB_NXTassettx,;

#endif
#else
#ifndef crypto777_db777_c
#define crypto777_db777_c

#ifndef crypto777_db777_h
#define DEFINES_ONLY
#include "db777.c"
#undef DEFINES_ONLY
#endif

void db777_lock(struct db777 *DB)
{
    if ( (DB->flags & DB777_MULTITHREAD) != 0 )
        portable_mutex_lock(&DB->mutex);
}

void db777_unlock(struct db777 *DB)
{
    if ( (DB->flags & DB777_MULTITHREAD) != 0 )
        portable_mutex_unlock(&DB->mutex);
}

void *db777_get(struct db777_entry **entryp,int32_t *lenp,void *transactions,struct db777 *DB,void *key,int32_t keylen)
{
    int32_t i,c; struct db777_entry *entry = 0; void *obj,*result = 0,*value = 0; char buf[8192],_keystr[513],*keystr = _keystr;
    if ( entryp != 0 )
        (*entryp = 0);
    if ( (DB->flags & DB777_RAM) != 0 )
    {
        db777_lock(DB);
        HASH_FIND(hh,DB->table,key,keylen,entry);
        db777_unlock(DB);
        if ( entry != 0 )
        {
            if ( entryp != 0 )
                (*entryp) = entry;
            *lenp = entry->valuelen;
            if ( entry->valuesize == 0 )
                memcpy(&value,entry->value,sizeof(value));
            else value = entry->value;
                return(value);
        }
    }
    if ( (DB->flags & DB777_HDD) != 0 )
    {
        if ( (obj= sp_object(DB->db)) != 0 )
        {
            if ( sp_set(obj,"key",key,keylen) == 0 )
            {
                if ( (result= sp_get(transactions != 0 ? transactions : DB->db,obj)) != 0 )
                    value = sp_get(result,"value",lenp);
            }
            if ( result != 0 )
                sp_destroy(result);
        }
        if ( value != 0 )
            return(value);
    }
    if ( (DB->flags & DB777_NANO) != 0 && DB->reqsock != 0 )
    {
        for (i=0; i<keylen; i++)
            if ( (c= ((uint8_t *)key)[i]) < 0x20 || c >= 0x80 )
                break;
        if ( i != keylen )
        {
            if ( keylen > sizeof(_keystr)/2 )
                key = malloc((keylen << 1) + 1);
            init_hexbytes_noT(keystr,key,keylen);
        }
        else
        {
            keystr = key;
            if ( keystr[keylen] != 0 )
            {
                printf("db777_get: adding null terminator\n");
                keystr[keylen] = 0;
            }
        }
        if ( keylen < sizeof(buf)-128 )
        {
            sprintf(buf,"{\"destplugin\":\"db777\",\"method\":\"get\",\"coin\":\"%s\",\"DB\":\"%s\",\"key\":\"%s\"}",DB->coinstr,DB->name,keystr);
            nn_send(DB->reqsock,buf,(int32_t)strlen(buf)+1,0);
            printf("db777_get: sent.(%s)\n",buf);
        } else printf("db777_get: keylen.%d too big for buf\n",keylen);
    }
    return(0);
}

void _db777_free(struct db777_entry *entry)
{
    void *obj;
    if ( entry->valuesize == 0 )
    {
        memcpy(&obj,entry->value,sizeof(obj));
        free(obj);
    }
    free(entry);
}

int32_t db777_delete(int32_t flags,void *transactions,struct db777 *DB,void *key,int32_t keylen)
{
    struct db777_entry *entry; void *obj; int32_t retval = -1;
    if ( DB != 0 )
    {
        if ( ((flags & DB->flags) & DB777_HDD) != 0 )
        {
            if ( (obj= sp_object(DB->db)) == 0 )
            {
                if ( sp_set(obj,"key",key,keylen) == 0 )
                    retval = sp_delete((transactions != 0) ? transactions : DB->db,obj);
                else sp_destroy(obj);
            }
        }
        if ( ((flags & DB->flags) & DB777_RAM) != 0 )
        {
            db777_lock(DB);
            HASH_FIND(hh,DB->table,key,keylen,entry);
            if ( entry != 0 )
            {
                HASH_DEL(DB->table,entry);
                _db777_free(entry);
            }
            db777_unlock(DB);
        }
    }
    return(retval);
}

void db777_free(struct db777 *DB)
{
    struct db777_entry *entry,*tmp;
    if ( DB->table != 0 )
    {
        db777_lock(DB);
        HASH_ITER(hh,DB->table,entry,tmp)
        {
            HASH_DEL(DB->table,entry);
            _db777_free(entry);
        }
        db777_unlock(DB);
    }
}

int32_t db777_set(int32_t flags,void *transactions,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t valuelen)
{
    struct db777_entry *entry = 0; void *db,*obj = 0; int32_t retval = -1;
    if ( ((DB->flags & flags) & DB777_HDD) != 0 )
    {
        db = DB->asyncdb != 0 ? DB->asyncdb : DB->db;
        if ( (obj= sp_object(db)) == 0 )
            retval = -3;
        if ( sp_set(obj,"key",key,keylen) != 0 || sp_set(obj,"value",value,valuelen) != 0 )
        {
            sp_destroy(obj);
            retval = -4;
        }
        else retval = sp_set((transactions != 0 ? transactions : db),obj);
        printf("HDD ret %d\n",retval);
    }
    if ( ((DB->flags & flags) & DB777_RAM) != 0 )
    {
        db777_lock(DB);
        HASH_FIND(hh,DB->table,key,keylen,entry);
        if ( entry == 0 )
        {
            if ( DB->valuesize == 0 )
            {
                entry = calloc(1,sizeof(*entry) + sizeof(void *));
                if ( valuelen > 0 )
                {
                    obj = malloc(valuelen);
                    memcpy(entry->value,&obj,sizeof(obj));
                }
            }
            else if ( valuelen == DB->valuesize )
            {
                entry = calloc(1,sizeof(*entry) + valuelen);
                obj = entry->value;
                entry->valuesize = DB->valuesize;
            }
            else
            {
                printf("mismatched valuelen.%d vs DB->valuesize.%d\n",valuelen,DB->valuesize);
                if ( obj != 0 && obj != entry->value )
                    free(obj);
                free(entry);
                db777_unlock(DB);
                return(-1);
            }
            entry->allocsize = entry->valuelen = valuelen;
            entry->keylen = keylen;
            entry->dirty = 1;
            memcpy(obj,value,valuelen);
            HASH_ADD_KEYPTR(hh,DB->table,key,keylen,entry);
            db777_unlock(DB);
            retval |= 1;
        }
        else
        {
            db777_unlock(DB);
            entry->dirty = (retval != 0);
            if ( entry->valuesize == 0 )
            {
                memcpy(&obj,entry->value,sizeof(obj));
                if ( entry->valuelen != valuelen || memcmp(obj,value,valuelen) != 0 )
                {
                    if ( entry->allocsize >= valuelen )
                    {
                        memcpy(obj,value,valuelen);
                        if ( valuelen < entry->allocsize )
                            memset(&((uint8_t *)obj)[valuelen],0,entry->allocsize - valuelen);
                    }
                    else
                    {
                        obj = realloc(obj,valuelen);
                        memcpy(obj,value,valuelen);
                        memcpy(entry->value,&obj,sizeof(obj));
                        entry->allocsize = valuelen;
                    }
                    entry->valuelen = valuelen;
                } else entry->dirty = 0;
                retval++;
            }
            else if ( entry->valuesize != valuelen || valuelen != DB->valuesize )
                printf("entry->valuesize.%d DB->valuesize.%d vs valuesize.%d??\n",entry->valuesize,DB->valuesize,valuelen);
            else
            {
                if ( memcmp(entry->value,value,valuelen) == 0 )
                    entry->dirty = 0;
                else memcpy(entry->value,value,valuelen);
                retval |= 1;
            }
        }
    }
    return(retval);
}

int32_t db777_flush(void *transactions,struct db777 *DB)
{
    struct db777_entry *entry,*tmp; void *obj; int32_t numerrs = 0;
    if ( (DB->flags & DB777_RAM) != 0 )
    {
        db777_lock(DB);
        HASH_ITER(hh,DB->table,entry,tmp)
        {
            if ( entry->valuesize == 0 && entry->valuelen < entry->valuesize )
            {
                memcpy(&obj,entry->value,sizeof(obj));
                entry->allocsize = entry->valuelen;
                obj = realloc(obj,entry->valuelen);
                memcpy(entry->value,&obj,sizeof(obj));
            }
            if ( entry->dirty != 0 )
            {
                if ( (DB->flags & DB777_HDD) != 0 )
                {
                    db777_delete(DB777_HDD,transactions,DB,entry->hh.key,entry->keylen);
                    entry->dirty = (db777_set(DB777_HDD,transactions,DB,entry->hh.key,entry->keylen,entry->value,entry->valuelen) != 0);
                    numerrs += entry->dirty;
                }
            }
        }
        db777_unlock(DB);
    }
    return(-numerrs);
}

uint32_t Duplicate,Mismatch,Added;
int32_t db777_add(int32_t forceflag,void *transactions,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t valuelen)
{
    void *val = 0;
    int32_t allocsize = 0;
    if ( DB == 0 )
        return(-1);
    if ( forceflag <= 0 && (val= db777_get(0,&allocsize,transactions,DB,key,keylen)) != 0 )
    {
        if ( allocsize == valuelen && memcmp(val,value,valuelen) == 0 )
        {
            if ( forceflag < 0 )
                Duplicate++;
            return(0);
        }
    }
    if ( forceflag < 0 && allocsize != 0 )
    {
        int i;
        for (i=0; i<60&&i<valuelen; i++)
            printf("%02x ",((uint8_t *)value)[i]);
        printf("value len.%d\n",valuelen);
        if ( val != 0 )
        {
            for (i=0; i<60&&i<allocsize; i++)
                printf("%02x ",((uint8_t *)val)[i]);
            printf("saved %d\n",allocsize);
        }
        Mismatch++, printf("duplicate.%d mismatch.%d | keylen.%d valuelen.%d -> allocsize.%d\n",Duplicate,Mismatch,keylen,valuelen,allocsize);
    }
    if ( forceflag < 1 )
        Added++;
    return(db777_set(DB->flags,transactions,DB,key,keylen,value,valuelen));
}

void *db777_findM(int32_t *lenp,void *transactions,struct db777 *DB,void *key,int32_t keylen) { return(db777_get(0,lenp,transactions,DB,key,keylen)); }

int32_t db777_addstr(struct db777 *DB,char *key,char *value)
{
    return(db777_add(1,0,DB,key,(int32_t)strlen(key)+1,value,(int32_t)strlen(value)+1));
}

int32_t db777_findstr(char *retbuf,int32_t max,struct db777 *DB,char *key)
{
    void *val;
    int32_t valuesize = -1;
    retbuf[0] = 0;
    if ( key == 0 || key[0] == 0 )
        return(-1);
    if ( (val= db777_get(0,&valuesize,0,DB,key,(int32_t)strlen(key)+1)) != 0 )
    {
        max--;
        if ( valuesize > 0 )
            memcpy(retbuf,val,(valuesize < max) ? valuesize : max), retbuf[max] = 0;
    }
    // printf("found str.(%s) -> (%s)\n",key,retbuf);
    return(valuesize);
}

int32_t db777_sync(struct env777 *DBs,int32_t flags)
{
    int32_t i,err = 0; void *transactions;
    if ( (flags & DB777_FLUSH) != 0 )
    {
        transactions = sp_begin(DBs->env);
        for (i=0; i<DBs->numdbs; i++)
            if ( (DBs->dbs[i].flags & DB777_FLUSH) != 0 )
                db777_flush(transactions,&DBs->dbs[i]);
        if ( (err= sp_commit(transactions)) != 0 )
            printf("db777_sync err.%d\n",err);
    }
    if ( (flags & ENV777_BACKUP) != 0 )
        sp_set(DBs->ctl,"backup.run");
    return(err);
}

uint64_t db777_ctlinfo64(void *ctl,char *field)
{
	void *obj,*ptr; uint64_t val = 0;
    if ( (obj= sp_get(ctl,field)) != 0 )
    {
        if ( (ptr= sp_get(obj,"value",NULL)) != 0 )
            val = calc_nxt64bits(ptr);
        sp_destroy(obj);
    }
    return(val);
}

char **db777_index(int32_t *nump,struct db777 *DB,int32_t max)
{
    void *obj,*cursor; uint32_t *addrindp; char *coinaddr,**coinaddrs = 0;
    int32_t addrlen,len,n = 0;
    *nump = 0;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    obj = sp_object(DB->db);
    if ( (cursor= sp_cursor(DB->db,obj)) != 0 )
    {
        coinaddrs = (char **)calloc(sizeof(char *),max+1);
        while ( (obj= sp_get(cursor,obj)) != 0 )
        {
            addrindp = sp_get(obj,"value",&len);
            coinaddr = sp_get(obj,"key",&addrlen);
            if ( len == sizeof(*addrindp) && *addrindp <= max )
            {
                coinaddrs[*addrindp] = calloc(1,addrlen + 1);
                memcpy(coinaddrs[*addrindp],coinaddr,addrlen);
                n++;
             } //else printf("n.%d wrong len.%d or overflow %d vs %d\n",n,len,*addrindp,max);
        }
        sp_destroy(cursor);
    }
    *nump = n;
    return(coinaddrs);
}

void **db777_copy_all(int32_t *nump,struct db777 *DB,char *field,int32_t size)
{
    void *obj,*cursor,*value,*ptr,**ptrs = 0;
    int32_t len,max,n = 0;
    *nump = 0;
    max = 100;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    obj = sp_object(DB->db);
    if ( (cursor= sp_cursor(DB->db,obj)) != 0 )
    {
        ptrs = (void **)calloc(sizeof(void *),max+1);
        while ( (obj= sp_get(cursor,obj)) != 0 )
        {
            value = sp_get(obj,field,&len);
            if ( len > 0 && (size == 0 || len == size) )
            {
                ptr = malloc(len);
                memcpy(ptr,value,len);
                //printf("%p set [%d] <- %p.len %d\n",ptrs,n,ptr,len);
                ptrs[n++] = ptr;
                if ( n >= max )
                {
                    max = n + 100;
                    ptrs = (void **)realloc(ptrs,sizeof(void *)*(max+1));
                }
            } //else printf("wrong size.%d\n",len);
        }
        sp_destroy(cursor);
    }
    if ( ptrs != 0 )
        ptrs[n] = 0;
    *nump = n;
    printf("ptrs.%p [0] %p numdb.%d\n",ptrs,ptrs[0],n);
    return(ptrs);
}

int32_t db777_dump(struct db777 *DB,int32_t binarykey,int32_t binaryvalue)
{
    void *obj,*cursor,*value,*key; char keyhex[8192],valuehex[8192];
    int32_t keylen,len,max,n = 0;
    max = 100;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    obj = sp_object(DB->db);
    if ( (cursor= sp_cursor(DB->db,obj)) != 0 )
    {
        while ( (obj= sp_get(cursor,obj)) != 0 )
        {
            value = sp_get(obj,"value",&len);
            key = sp_get(obj,"key",&keylen);
            if ( binarykey != 0 )
                init_hexbytes_noT(keyhex,key,keylen), key = keyhex;
            if ( binaryvalue != 0 )
                init_hexbytes_noT(valuehex,value,len), value = valuehex;
            printf("%-5d: %100s | %s\n",n,key,value);
            n++;
        }
        sp_destroy(cursor);
    }
    return(n);
}

int32_t eligible_lbserver(char *server)
{
    cJSON *json; int32_t len,keylen,retval = 1; char *jsonstr,*status,*valstr = "{\"status\":\"enabled\"}";
    if ( server == 0 || server[0] == 0 || ismyaddress(server) != 0 || is_remote_access(server) == 0 )
        return(0);
    keylen = (int32_t)strlen(server)+1;
    if ( (jsonstr= db777_findM(&len,0,DB_NXTaccts,(uint8_t *)server,keylen)) != 0 )
    {
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (status= cJSON_str(cJSON_GetObjectItem(json,"status"))) != 0 )
            {
                //printf("(%s) status.(%s)\n",server,status);
                if ( strcmp(status,"disabled") != 0 )
                    retval = 0;
            }
            free_json(json);
        }
        free(jsonstr);
    }
    else db777_add(1,0,DB_NXTaccts,server,keylen,valstr,(int32_t)strlen(valstr)+1);
    return(retval);
}

#endif
#endif
