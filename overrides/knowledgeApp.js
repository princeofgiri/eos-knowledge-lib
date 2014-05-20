const Gtk = imports.gi.Gtk;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const Endless = imports.gi.Endless;
const EosKnowledge = imports.gi.EosKnowledge;
const Lang = imports.lang;

const ENDLESS_PREFIX = '/com/endlessm/';

const KnowledgeApp = new Lang.Class ({
    Name: 'KnowledgeApp',
    GTypeName: 'EknKnowledgeApp',
    Extends: Endless.Application,

    _init: function(props, gresource_filename){
        this.parent(props);
        this._gresource_filename = gresource_filename;
    },

    vfunc_startup: function() {
        this.parent();
        let provider = new Gtk.CssProvider();
        let css_file = Gio.File.new_for_uri('resource:///com/endlessm/knowledge/endless_knowledge.css');
        provider.load_from_file(css_file);
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),
                                                 provider,
                                                 Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

        // Load and register the GResource which has content for this app
        let resource = Gio.Resource.load(this._gresource_filename);
        resource._register();

        // Parse the appname and personality from the gresource
        let appname = resource.enumerate_children(ENDLESS_PREFIX, Gio.FileQueryInfoFlags.NONE, null)[0];
        let app_json_file_uri = 'resource://' + ENDLESS_PREFIX + appname + 'app.json';

        let win = new EosKnowledge.WindowA({
            application: this
        });

        let presenter = new EosKnowledge.Presenter({
            view: win
        }, app_json_file_uri);

        win.show_all();
    }
});
