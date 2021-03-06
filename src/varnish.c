/**
 * collectd - src/varnish.c
 * Copyright (C) 2010 Jerome Renard
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Jerome Renard <jerome.renard@gmail.com>
 **/

/**
 * Current list of what is monitored and what is not monitored (yet)
 *
 * Field name           Description                           Monitored
 * ----------           -----------                           ---------
 * uptime               Child uptime                              N
 * client_conn          Client connections accepted               Y
 * client_drop          Connection dropped, no sess               Y
 * client_req           Client requests received                  Y
 * cache_hit            Cache hits                                Y
 * cache_hitpass        Cache hits for pass                       Y
 * cache_miss           Cache misses                              Y
 * backend_conn         Backend conn. success                     Y
 * backend_unhealthy    Backend conn. not attempted               Y
 * backend_busy         Backend conn. too many                    Y
 * backend_fail         Backend conn. failures                    Y
 * backend_reuse        Backend conn. reuses                      Y
 * backend_toolate      Backend conn. was closed                  Y
 * backend_recycle      Backend conn. recycles                    Y
 * backend_unused       Backend conn. unused                      Y
 * fetch_head           Fetch head                                Y
 * fetch_length         Fetch with Length                         Y
 * fetch_chunked        Fetch chunked                             Y
 * fetch_eof            Fetch EOF                                 Y
 * fetch_bad            Fetch had bad headers                     Y
 * fetch_close          Fetch wanted close                        Y
 * fetch_oldhttp        Fetch pre HTTP/1.1 closed                 Y
 * fetch_zero           Fetch zero len                            Y
 * fetch_failed         Fetch failed                              Y
 * n_sess_mem           N struct sess_mem                         N
 * n_sess               N struct sess                             N
 * n_object             N struct object                           N
 * n_vampireobject      N unresurrected objects                   N
 * n_objectcore         N struct objectcore                       N
 * n_objecthead         N struct objecthead                       N
 * n_smf                N struct smf                              N
 * n_smf_frag           N small free smf                          N
 * n_smf_large          N large free smf                          N
 * n_vbe_conn           N struct vbe_conn                         N
 * n_wrk                N worker threads                          N
 * n_wrk_create         N worker threads created                  N
 * n_wrk_failed         N worker threads not created              N
 * n_wrk_max            N worker threads limited                  N
 * n_wrk_queue          N queued work requests                    N
 * n_wrk_overflow       N overflowed work requests                N
 * n_wrk_drop           N dropped work requests                   N
 * n_backend            N backends                                N
 * n_expired            N expired objects                         N
 * n_lru_nuked          N LRU nuked objects                       N
 * n_lru_saved          N LRU saved objects                       N
 * n_lru_moved          N LRU moved objects                       N
 * n_deathrow           N objects on deathrow                     N
 * losthdr              HTTP header overflows                     N
 * n_objsendfile        Objects sent with sendfile                N
 * n_objwrite           Objects sent with write                   N
 * n_objoverflow        Objects overflowing workspace             N
 * s_sess               Total Sessions                            N
 * s_req                Total Requests                            N
 * s_pipe               Total pipe                                N
 * s_pass               Total pass                                N
 * s_fetch              Total fetch                               N
 * s_hdrbytes           Total header bytes                        N
 * s_bodybytes          Total body bytes                          N
 * sess_closed          Session Closed                            N
 * sess_pipeline        Session Pipeline                          N
 * sess_readahead       Session Read Ahead                        N
 * sess_linger          Session Linger                            N
 * sess_herd            Session herd                              N
 * shm_records          SHM records                               Y
 * shm_writes           SHM writes                                Y
 * shm_flushes          SHM flushes due to overflow               Y
 * shm_cont             SHM MTX contention                        Y
 * shm_cycles           SHM cycles through buffer                 Y
 * sm_nreq              allocator requests                        Y
 * sm_nobj              outstanding allocations                   Y
 * sm_balloc            bytes allocated                           Y
 * sm_bfree             bytes free                                Y
 * sma_nreq             SMA allocator requests                    Y
 * sma_nobj             SMA outstanding allocations               Y
 * sma_nbytes           SMA outstanding bytes                     Y
 * sma_balloc           SMA bytes allocated                       Y
 * sma_bfree            SMA bytes free                            Y
 * sms_nreq             SMS allocator requests                    Y
 * sms_nobj             SMS outstanding allocations               Y
 * sms_nbytes           SMS outstanding bytes                     Y
 * sms_balloc           SMS bytes allocated                       Y
 * sms_bfree            SMS bytes freed                           Y
 * backend_req          Backend requests made                     N
 * n_vcl                N vcl total                               N
 * n_vcl_avail          N vcl available                           N
 * n_vcl_discard        N vcl discarded                           N
 * n_purge              N total active purges                     N
 * n_purge_add          N new purges added                        N
 * n_purge_retire       N old purges deleted                      N
 * n_purge_obj_test     N objects tested                          N
 * n_purge_re_test      N regexps tested against                  N
 * n_purge_dups         N duplicate purges removed                N
 * hcb_nolock           HCB Lookups without lock                  Y
 * hcb_lock             HCB Lookups with lock                     Y
 * hcb_insert           HCB Inserts                               Y
 * esi_parse            Objects ESI parsed (unlock)               Y
 * esi_errors           ESI parse errors (unlock)                 Y
 */
#include "collectd.h"
#include "common.h"
#include "plugin.h"
#include "configfile.h"

#include <varnish/varnishapi.h>

#if HAVE_VARNISH_V3
	# include <varnish/vsc.h>
	typedef struct VSC_C_main c_varnish_stats_t;
#endif

#if HAVE_VARNISH_V2
	typedef struct varnish_stats c_varnish_stats_t;
#endif

/* {{{ user_config_s */
struct user_config_s {
	char *instance;

	_Bool monitor_cache;
	_Bool monitor_connections;
	_Bool monitor_esi;
	_Bool monitor_backend;
	_Bool monitor_fetch;
	_Bool monitor_hcb;
	_Bool monitor_shm;
	_Bool monitor_sms;
#if HAVE_VARNISH_V2
	_Bool monitor_sm;
	_Bool monitor_sma;
#endif
	_Bool monitor_totals;
	_Bool monitor_workers;
};
typedef struct user_config_s user_config_t; /* }}} */

static _Bool have_instance = 0;

static void varnish_submit (const char *plugin_instance, /* {{{ */
		const char *type, const char *type_instance, gauge_t value)
{
	value_t values[1];
	value_list_t vl = VALUE_LIST_INIT;

	if (strcasecmp ("gauge", type) == 0)
		values[0].gauge = value;
	else
		values[0].counter = value;
	vl.values_len = 1;
	vl.values = values;

	sstrncpy(vl.host         , hostname_g   , sizeof(vl.host));
	sstrncpy(vl.plugin       , "varnish"    , sizeof(vl.plugin));
	if (plugin_instance != NULL)
		sstrncpy (vl.plugin_instance, plugin_instance,
				sizeof (vl.plugin_instance));
	sstrncpy(vl.type         , type         , sizeof(vl.type));
	sstrncpy(vl.type_instance, type_instance, sizeof(vl.type_instance));

	plugin_dispatch_values(&vl);
} /* }}} */

static void varnish_monitor (const user_config_t *conf, /* {{{ */
	const c_varnish_stats_t *stats)
{
	if(conf->monitor_cache)
	{
		varnish_submit (conf->instance, "derive", "cache_hit"    , stats->cache_hit);     /* Cache hits          */
		varnish_submit (conf->instance, "derive", "cache_miss"   , stats->cache_miss);    /* Cache misses        */
		varnish_submit (conf->instance, "derive", "cache_hitpass", stats->cache_hitpass); /* Cache hits for pass */
	}

	if(conf->monitor_connections)
	{
		varnish_submit (conf->instance, "derive", "client_connections-accepted", stats->client_conn); /* Client connections accepted */
		varnish_submit (conf->instance, "derive", "client_connections-dropped" , stats->client_drop); /* Connection dropped, no sess */
		varnish_submit (conf->instance, "derive", "client_requests-received", stats->client_req);  /* Client requests received    */
	}

	if(conf->monitor_esi)
	{
		varnish_submit (conf->instance, "derive", "esi_errors", stats->esi_errors); /* ESI parse errors (unlock)   */
#if HAVE_VARNISH_V2
		varnish_submit (conf->instance, "derive", "esi_parsed", stats->esi_parse);  /* Objects ESI parsed (unlock) */
#endif
	}

	if(conf->monitor_backend)
	{
		varnish_submit (conf->instance, "derive", "backend_connections-success"      , stats->backend_conn);      /* Backend conn. success       */
		varnish_submit (conf->instance, "derive", "backend_connections-not-attempted", stats->backend_unhealthy); /* Backend conn. not attempted */
		varnish_submit (conf->instance, "derive", "backend_connections-too-many"     , stats->backend_busy);      /* Backend conn. too many      */
		varnish_submit (conf->instance, "derive", "backend_connections-failures"     , stats->backend_fail);      /* Backend conn. failures      */
		varnish_submit (conf->instance, "derive", "backend_connections-reuses"       , stats->backend_reuse);     /* Backend conn. reuses        */
		varnish_submit (conf->instance, "derive", "backend_connections-was-closed"   , stats->backend_toolate);   /* Backend conn. was closed    */
		varnish_submit (conf->instance, "derive", "backend_connections-recycles"     , stats->backend_recycle);   /* Backend conn. recycles      */
#if HAVE_VARNISH_V2
		varnish_submit (conf->instance, "derive", "backend_connections-unused"       , stats->backend_unused);    /* Backend conn. unused        */
#endif
	}

	if(conf->monitor_fetch)
	{
		varnish_submit (conf->instance, "derive", "fetch_head"       , stats->fetch_head);    /* Fetch head                */
		varnish_submit (conf->instance, "derive", "fetch_length"     , stats->fetch_length);  /* Fetch with length         */
		varnish_submit (conf->instance, "derive", "fetch_chunked"    , stats->fetch_chunked); /* Fetch chunked             */
		varnish_submit (conf->instance, "derive", "fetch_eof"        , stats->fetch_eof);     /* Fetch EOF                 */
		varnish_submit (conf->instance, "derive", "fetch_bad-headers", stats->fetch_bad);     /* Fetch bad headers         */
		varnish_submit (conf->instance, "derive", "fetch_close"      , stats->fetch_close);   /* Fetch wanted close        */
		varnish_submit (conf->instance, "derive", "fetch_oldhttp"    , stats->fetch_oldhttp); /* Fetch pre HTTP/1.1 closed */
		varnish_submit (conf->instance, "derive", "fetch_zero"       , stats->fetch_zero);    /* Fetch zero len            */
		varnish_submit (conf->instance, "derive", "fetch_failed"     , stats->fetch_failed);  /* Fetch failed              */
	}

	if(conf->monitor_hcb)
	{
		varnish_submit (conf->instance, "derive", "hcb_nolock", stats->hcb_nolock); /* HCB Lookups without lock */
		varnish_submit (conf->instance, "derive", "hcb_lock"  , stats->hcb_lock);   /* HCB Lookups with lock    */
		varnish_submit (conf->instance, "derive", "hcb_insert", stats->hcb_insert); /* HCB Inserts              */
	}

	if(conf->monitor_shm)
	{
		varnish_submit (conf->instance, "derive", "shm_records"   , stats->shm_records); /* SHM records                 */
		varnish_submit (conf->instance, "derive", "shm_writes"    , stats->shm_writes);  /* SHM writes                  */
		varnish_submit (conf->instance, "derive", "shm_flushes"   , stats->shm_flushes); /* SHM flushes due to overflow */
		varnish_submit (conf->instance, "derive", "shm_contention", stats->shm_cont);    /* SHM MTX contention          */
		varnish_submit (conf->instance, "derive", "shm_cycles"    , stats->shm_cycles);  /* SHM cycles through buffer   */
	}

#if HAVE_VARNISH_V2
	if(conf->monitor_sm)
	{
		varnish_submit (conf->instance, "derive", "sm_nreq"  , stats->sm_nreq);   /* allocator requests      */
		varnish_submit (conf->instance, "derive", "sm_nobj"  , stats->sm_nobj);   /* outstanding allocations */
		varnish_submit (conf->instance, "gauge", "sm_balloc", stats->sm_balloc); /* bytes allocated         */
		varnish_submit (conf->instance, "gauge", "sm_bfree" , stats->sm_bfree);  /* bytes free              */
	}

	if(conf->monitor_sma)
	{
		varnish_submit (conf->instance, "derive", "sma_req"   , stats->sma_nreq);   /* SMA allocator requests      */
		varnish_submit (conf->instance, "derive", "sma_nobj"  , stats->sma_nobj);   /* SMA outstanding allocations */
		varnish_submit (conf->instance, "gauge", "sma_nbytes", stats->sma_nbytes); /* SMA outstanding bytes       */
		varnish_submit (conf->instance, "gauge", "sma_balloc", stats->sma_balloc); /* SMA bytes allocated         */
		varnish_submit (conf->instance, "gauge", "sma_bfree" , stats->sma_bfree);  /* SMA bytes free              */
	}
#endif

	if(conf->monitor_sms)
	{
		varnish_submit (conf->instance, "derive", "sms_nreq"  , stats->sms_nreq);   /* SMS allocator requests      */
		varnish_submit (conf->instance, "derive", "sms_nobj"  , stats->sms_nobj);   /* SMS outstanding allocations */
		varnish_submit (conf->instance, "gauge", "sms_nbytes", stats->sms_nbytes); /* SMS outstanding bytes       */
		varnish_submit (conf->instance, "gauge", "sms_balloc", stats->sms_balloc); /* SMS bytes allocated         */
		varnish_submit (conf->instance, "gauge", "sms_bfree" , stats->sms_bfree);  /* SMS bytes freed             */
	}

        if (conf->monitor_totals)
        {
                /* Total Sessions */
                varnish_submit (conf->instance, "derive", "sessions",  stats->s_sess);
                /* Total Requests */
                varnish_submit (conf->instance, "derive", "requests",  stats->s_req);
                /* Total pipe */
                varnish_submit (conf->instance, "derive", "pipe",    stats->s_pipe);
                /* Total pass */
                varnish_submit (conf->instance, "derive", "pass",    stats->s_pass);
                /* Total fetch */
                varnish_submit (conf->instance, "derive", "fetches", stats->s_fetch);
                /* Total header bytes */
                varnish_submit (conf->instance, "derive", "header-bytes", stats->s_hdrbytes);
                /* Total body byte */
                varnish_submit (conf->instance, "derive", "body-bytes",   stats->s_bodybytes);
        }

        if (conf->monitor_workers)
        {
                /* worker threads */
                varnish_submit (conf->instance, "gauge", "threads",            stats->n_wrk);
                /* worker threads created */
                varnish_submit (conf->instance, "derive", "threads_created",     stats->n_wrk_create);
                /* worker threads not created */
                varnish_submit (conf->instance, "derive", "threads_failed",      stats->n_wrk_failed);
                /* worker threads limited */
                varnish_submit (conf->instance, "derive", "threads_limited",     stats->n_wrk_max);
                varnish_submit (conf->instance, "derive", "requests_dropped",    stats->n_wrk_drop);
#ifdef HAVE_VARNISH_V2
                /* queued work requests */
                varnish_submit (conf->instance, "derive", "requests_queued",     stats->n_wrk_queue);
                /* overflowed work requests */
                varnish_submit (conf->instance, "derive", "requests_overflowed", stats->n_wrk_overflow);
#endif
                /* dropped work requests */
        }

} /* }}} */

#if HAVE_VARNISH_V3
static int varnish_read (user_data_t *ud) /* {{{ */
{
  struct VSM_data *vd;
  const c_varnish_stats_t *stats;

  user_config_t *conf;

  if ((ud == NULL) || (ud->data == NULL))
    return (EINVAL);

  conf = ud->data;

  vd = VSM_New();
  VSC_Setup(vd);

  if (VSC_Open (vd, /* diag = */ 1))
  {
    ERROR ("varnish plugin: Unable to load statistics.");

    return (-1);
  }

  stats = VSC_Main(vd);

  varnish_monitor (conf, stats);
  VSM_Close (vd);

  return (0);
} /* }}} */
#else /* if HAVE_VARNISH_V2 */

static int varnish_read(user_data_t *ud) /* {{{ */
{
	const c_varnish_stats_t *stats;
	user_config_t *conf;

	if ((ud == NULL) || (ud->data == NULL))
		return (EINVAL);

	conf = ud->data;

	stats = VSL_OpenStats (conf->instance);
	  if (stats == NULL)
	{
		ERROR("Varnish plugin : unable to load statistics");

		return (-1);
	}

	varnish_monitor (conf, stats);

    return (0);
} /* }}} */
#endif

static void varnish_config_free (void *ptr) /* {{{ */
{
	user_config_t *conf = ptr;

	if (conf == NULL)
		return;

	sfree (conf->instance);
	sfree (conf);
} /* }}} */

static int varnish_init (void) /* {{{ */
{
	user_config_t *conf;
	user_data_t ud;

	if (have_instance)
		return (0);

	conf = malloc (sizeof (*conf));
	if (conf == NULL)
		return (ENOMEM);
	memset (conf, 0, sizeof (*conf));

	/* Default settings: */
	conf->instance = NULL;
	conf->monitor_cache = 1;
	conf->monitor_backend = 1;
	conf->monitor_connections = 1;
	conf->monitor_shm = 1;

	ud.data = conf;
	ud.free_func = varnish_config_free;

	plugin_register_complex_read (/* group = */ "varnish",
			/* name      = */ "varnish/localhost",
			/* callback  = */ varnish_read,
			/* interval  = */ NULL,
			/* user data = */ &ud);

	return (0);
} /* }}} int varnish_init */

static int varnish_config_instance (const oconfig_item_t *ci) /* {{{ */
{
	user_config_t *conf;
	user_data_t ud;
	char callback_name[DATA_MAX_NAME_LEN];
	int i;

	conf = malloc (sizeof (*conf));
	if (conf == NULL)
		return (ENOMEM);
	memset (conf, 0, sizeof (*conf));
	conf->instance = NULL;

	if (ci->values_num == 1)
	{
		int status;

		status = cf_util_get_string (ci, &conf->instance);
		if (status != 0)
		{
			sfree (conf);
			return (status);
		}
		assert (conf->instance != NULL);

		if (strcmp ("localhost", conf->instance) == 0)
		{
			sfree (conf->instance);
			conf->instance = NULL;
		}
	}
	else if (ci->values_num > 1)
	{
		WARNING ("Varnish plugin: \"Instance\" blocks accept only "
				"one argument.");
		return (EINVAL);
	}

	for (i = 0; i < ci->children_num; i++)
	{
		oconfig_item_t *child = ci->children + i;

		if (strcasecmp ("CollectCache", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_cache);
		else if (strcasecmp ("CollectConnections", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_connections);
		else if (strcasecmp ("CollectESI", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_esi);
		else if (strcasecmp ("CollectBackend", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_backend);
		else if (strcasecmp ("CollectFetch", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_fetch);
		else if (strcasecmp ("CollectHCB", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_hcb);
		else if (strcasecmp ("CollectSHM", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_shm);
		else if (strcasecmp ("CollectSMS", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_sms);
#if HAVE_VARNISH_V2
		else if (strcasecmp ("CollectSMA", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_sma);
		else if (strcasecmp ("CollectSM", child->key) == 0)
			cf_util_get_boolean (child, &conf->monitor_sm);
#endif
		else if (strcasecmp ("CollectTotals", child->key) == 0)
                        cf_util_get_boolean (child, &conf->monitor_totals);
                else if (strcasecmp ("CollectWorkers", child->key) == 0)
                        cf_util_get_boolean (child, &conf->monitor_workers);
		else
		{
			WARNING ("Varnish plugin: Ignoring unknown "
					"configuration option: \"%s\"",
					child->key);
		}
	}

	if (!conf->monitor_cache
			&& !conf->monitor_connections
			&& !conf->monitor_esi
			&& !conf->monitor_backend
			&& !conf->monitor_fetch
			&& !conf->monitor_hcb
			&& !conf->monitor_shm
			&& !conf->monitor_sms
#if HAVE_VARNISH_V2
			&& !conf->monitor_sma
			&& !conf->monitor_sm
#endif
			&& !conf->monitor_totals
                        && !conf->monitor_workers)
	{
		WARNING ("Varnish plugin: No metric has been configured for "
				"instance \"%s\". Disabling this instance.",
				(conf->instance == NULL) ? "localhost" : conf->instance);
		return (EINVAL);
	}

	ssnprintf (callback_name, sizeof (callback_name), "varnish/%s",
			(conf->instance == NULL) ? "localhost" : conf->instance);

	ud.data = conf;
	ud.free_func = varnish_config_free;

	plugin_register_complex_read (/* group = */ "varnish",
			/* name      = */ callback_name,
			/* callback  = */ varnish_read,
			/* interval  = */ NULL,
			/* user data = */ &ud);

	have_instance = 1;

	return (0);
} /* }}} int varnish_config_instance */

static int varnish_config (oconfig_item_t *ci) /* {{{ */
{
	int i;

	for (i = 0; i < ci->children_num; i++)
	{
		oconfig_item_t *child = ci->children + i;

		if (strcasecmp ("Instance", child->key) == 0)
			varnish_config_instance (child);
		else
		{
			WARNING ("Varnish plugin: Ignoring unknown "
					"configuration option: \"%s\"",
					child->key);
		}
	}

	return (0);
} /* }}} int varnish_config */

void module_register (void) /* {{{ */
{
	plugin_register_complex_config("varnish", varnish_config);
	plugin_register_init ("varnish", varnish_init);
} /* }}} */

/* vim: set sw=8 noet fdm=marker : */
