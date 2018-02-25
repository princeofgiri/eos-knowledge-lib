const Eknc = imports.gi.EosKnowledgeContent;
const Eknr = imports.gi.EosKnowledgeRenderer;
const Endless = imports.gi.Endless;
const Gettext = imports.gettext;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;

const Config = imports.app.config;
const Knowledge = imports.app.knowledge;
const Mustache = imports.app.libs.mustache.Mustache;
const Utils = imports.app.utils;
const SetMap = imports.app.setMap;

let _ = Gettext.dgettext.bind(null, Config.GETTEXT_PACKAGE);

function template_file(template_filename) {
    return Gio.file_new_for_uri('resource:///com/endlessm/knowledge/data/templates/' + template_filename);
}

/**
 * Class: ArticleHTMLRenderer
 */
var ArticleHTMLRenderer = new Knowledge.Class({
    Name: "ArticleHTMLRenderer",
    Extends: GObject.Object,

    Properties: {
        /**
         * Property: show-title
         * True if the article title should be rendered inside the web page.
         */
        'show-title': GObject.ParamSpec.boolean('show-title',
           'Show Title', 'Show Title',
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT,
            false),
        /**
         * Property: enable-scroll-manager
         * True if the web side javascript to scroll with the table of contents
         * should be injected.
         */
        'enable-scroll-manager': GObject.ParamSpec.boolean('enable-scroll-manager',
           'Enable Scroll Manager', 'Enable Scroll Manager',
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT,
            false),
    },

    _init: function (props={}) {
        this.parent(props);
        this._renderer = new Eknr.Renderer({});
        this._custom_css_files = [];
    },

    set_custom_css_files: function (custom_css_files) {
        this._custom_css_files = custom_css_files;
    },

    _get_app_override_css_files: function () {
        let app = Gio.Application.get_default();
        return app.get_web_overrides_css();
    },

    _get_html: function (model) {
        let [success, bytes, mime] = Eknc.Engine.get_default().get_domain().read_uri(model.ekn_id);
        return bytes.get_data().toString();
    },

    _render_content: function (model) {
        // We have two code paths here. Newer content is server-templated, which means that
        // the HTML content in the web view has been pre-styled and we don't need much
        // additional processing.
        //
        // "Legacy" content, including most wiki sources and also 2.6 Prensa Libre content,
        // is templated and styled on the client.

        return this._renderer.render_content(this._get_html(model),
                                             model.is_server_templated,
                                             model.source,
                                             model.source_name,
                                             model.original_uri,
                                             model.license,
                                             model.title,
                                             this.show_title,
                                             this.enable_scroll_manager);
    },

    _render_wrapper: function (content, model) {
        let setMap = null;
        let tagMap = SetMap.get_tag_map();
        let engine = Eknc.Engine.get_default();

        if (tagMap) {
          setMap = [...tagMap.keys()].reduce((acc, key) => {
              let set = tagMap.get(key);
              acc[key] = Eknr.BoxedSetInfo.new(set.featured,
                                               set.ekn_id,
                                               set.title,
                                               set.child_tags,
                                               set.tags);
              return acc;
          }, {});
        } else {
          setMap = {};
        }

        return this._renderer.render_wrapper(content,
                                             model.ekn_id,
                                             model.title,
                                             model.published,
                                             model.authors,
                                             model.license,
                                             model.source,
                                             model.source_name,
                                             model.original_uri,
                                             model.tags,
                                             setMap,
                                             model.outgoing_links,
                                             (l) => engine.test_link(l),
                                             this._custom_css_files,
                                             this._get_app_override_css_files());
    },

    /*
     * The render method is called with an article model :model and returns a
     * string of ready to display html.
     */
    render: function (model) {
        let content = this._render_content(model);
        return this._render_wrapper(content, model);
    },
});

function _get_display_string_for_license(license) {
    if (license === Endless.LICENSE_NO_LICENSE)
        // TRANSLATORS: the text inside curly braces {blog-link} is going to be
        // substituted in code. Please make sure that your translation contains
        // {blog-link} and it is not translated.
        return _("Content taken from {blog-link}.");
    if (license === Endless.LICENSE_OWNER_PERMISSION)
        // TRANSLATORS: the text inside curly braces {blog-link} is going to be
        // substituted in code. Please make sure that your translation contains
        // {blog-link} and it is not translated.
        return _("Content courtesy of {blog-link}. Used with kind permission.");

    let license_link = _to_license_link(license);
    // TRANSLATORS: the text inside curly braces ({blog-link}, {license-link})
    // is going to be substituted in code. Please make sure that your
    // translation contains both {blog-link} and {license-link} and they are not
    // translated.
    return _("Content courtesy of {blog-link}, licensed under {license-link}.")
        .replace('{license-link}', license_link);
}
