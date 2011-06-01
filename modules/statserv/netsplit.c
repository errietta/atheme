/*
 * Copyright (c) 2011 Alexandria Wolcott
 * Released under the same terms as Atheme itself.
 *
 * Netsplit monitor
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1("statserv/netsplit", false, _modinit, _moddeinit,
          PACKAGE_STRING, "Alexandria Wolcott <alyx@sporksmoo.net>");

static void ss_cmd_netsplit(sourceinfo * si, int parc, char *parv[]);
static void ss_cmd_netsplit_list(sourceinfo * si, int parc, char *parv[]);
static void ss_cmd_netsplit_remove(sourceinfo * si, int parc, char *parv[]);

command_t ss_netsplit =
    { "NETSPLIT", N_("Monitor network splits."), AC_NONE, 2, ss_cmd_netsplit, {.path = "statserv/netsplit"} };

command_t ss_netsplit_list =
    { "LIST", N_("List currently split servers."), PRIV_SERVER_AUSPEX, 1, ss_cmd_netsplit_list, {.path = ""} };

command_t ss_netsplit_remove = 
    { "REMOVE", N_("Remove a server from the netsplit list."), PRIV_JUPE, 1, ss_cmd_netsplit_remove, {.path = ""} };

mowgli_patricia_t *ss_netsplit_cmds;
mowgli_patricia_t *splitlist;
mowgli_heap_t *split_heap;

static void split_find(const char *name)
{
    return mowgli_patricia_retrieve(splitlist, name);
}

static void netsplit_server_add(server_t *s)
{
    if (split_find(s->name))
    {
        /* Insert magical code to remove a netsplit here. */
        mowgli_patricia_delete(splitlist, s->name);
        if (s->uplink)
            wallops("Netsplit between %s and %s over.", s->name, s->uplink->name);
        else
            wallops("Netsplit from %s over.", s->name);
    }
}

static void netsplit_server_delete(server_t *serv)
{
    server_t *s;
    s = mowgli_heap_alloc(split_heap);
    s->sid = serv->sid;
    s->name = serv->name;
    s->desc = serv->desc;
    s->connected_since = CURRTIME;
    if (serv->uplink)
        s->uplink = serv->uplink;
    s->hops = serv->hops;
    mowgli_patricia_add(splitlist, s->name, s);
    if (s->uplink)
        wallops("Server %s split from %s.", s->name, s->uplink->name);
    else
        wallops("Server %s split from the network.", s->name);
}

static void ss_cmd_netsplit(sourceinfo_t * si, int parc, char *parv[])
{
    command_t *c;
    char *cmd = parv[0];

    if (!cmd)
    {
        command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NETSPLIT");
        command_fail(si, fault_needmoreparams,
                _("Syntax: NETSPLIT [LIST|REMOVE] [parameters]"));
        return;
    }

    c = command_find(ss_netsplit_cmds, cmd);
    if (c == NULL)
    {
        command_fail(sim fault_badparams,
                _("Invalid command. Use \2/%s%s help\2 for a command listing."),
                (ircd->uses_rcommand == false) ? "msg " : "", si->service->disp);
        return;
    }
    
    command_exec(si->service, si, c, parc - 1, parv + 1);
}

static void ss_cmd_netsplit_list(sourceinfo_t * si, int parc, char *parv[])
{
    server_t *s;
    mowgli_patricia_iteration_state_t state;
    int i = 0;
    
    MOWGLI_PATRICIA_FOREACH(s, &state, splitlist)
    {
        i++;
        command_success_nodata(si, _("%d: %s [Split %s ago]"), i, s->name, time_ago(s->connected_since));
    }
    command_success_nodata(si, _("End of netsplit list."));
}

static void ss_cmd_netsplit_remove(sourceinfo_t * si, int parc, char *parv[])
{
    char *name = parv[0];
    if (mowgli_patricia_delete(splitlist, name))
        command_success_nodata(si, _("%s removed from the netsplit list."), name);
    else
        command_fail(si, fault_nosuch_target, _("The server \2%s\2 does is not a split server."), name);
}

void _modinit(module_t * m)
{
    service_named_bind_command("statserv", &ss_netsplit);

    ss_netsplit_cmds = mowgli_patricia_create(strcasecanon);
    
    command_add(&ss_netsplit_list, ss_netsplit_cmds);
    command_add(&ss_netsplit_remove, ss_netsplit_cmds);

    hook_add_event("server_add");
    hook_add_event("server_delete");
    hook_add_server_add(netsplit_server_add);
    hook_add_server_delete(netsplit_server_delete);

    split_heap = mowgli_heap_create(sizeof(server_t), HEAP_SERVER, BH_NOW);

    if (split_heap == NULL)
    {
        slog(LG_INFO, "statserv/netsplit _modinit(): block allocator failure.");
        exit(EXIT_FAILURE);
    }

    splitlist = mowgli_patricia_create(irccasecanon);
}

void _moddeinit(module_unload_intent_t intent)
{
   service_named_unbind_command("statserv", &ss_netsplit);

   command_delete(&ss_netsplit_list, ss_netsplit_cmds);
   command_delete(&ss_netsplit_remove, ss_netsplit_cmds);

   hook_del_event("server_add");
   hook_del_event("server_delete");
   hook_del_server_add(netsplit_server_add);
   hook_del_server_delete(netsplit_server_delete);

   mowgli_patricia_destroy(ss_netsplit_cmds, NULL, NULL);
   mowgli_patricia_destroy(splitlist, NULL, NULL);
}
