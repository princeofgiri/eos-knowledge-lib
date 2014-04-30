// Copyright 2014 Endless Mobile, Inc.

const EosKnowledge = imports.gi.EosKnowledge;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;

/**
 * Class: SectionPage
 *
 * This represents the section page for template A of the knowledge apps.
 * It will also be used as the search results page.
 * It has a title and a set of articles to show. Articles are represented
 * by cards. Cards are grouped into sections call 'Segments'. A segment has
 * a title, which is the type of cards in its section, and a list of cards
 * to display.
 *
 */
const SectionPageA = new Lang.Class({
    Name: 'SectionPageA',
    GTypeName: 'EknSectionPageA',
    Extends: Gtk.ScrolledWindow,
    Properties: {
        /**
         * Property: title
         * A string with the title of the section page. Defaults to an empty string.
         */
        'title': GObject.ParamSpec.string('title', 'Page Title',
            'Title of the page',
            GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE, ''),
        /**
         * Property: segments
         * An object where keys are a string label representing a card type, e.g. 'Lesson',
         * and values are a list of <Card> widgets associated with that type. Each key-value
         * pair is converted into a 'CardsSegment' in the setter. SectionPage maintains a list of
         * card segments internally.
         */
    },

    _init: function (props) {
        props = props || {};
        props.hscrollbar_policy = Gtk.PolicyType.NEVER;

        this._content_grid = new Gtk.Grid({
            orientation: Gtk.Orientation.VERTICAL,
            expand: true,
            valign: Gtk.Align.START,
            row_spacing: 20,
            margin: 40
        });

        this._title_label = new Gtk.Label();

        this._segments = null;
        this._title = null;

        this.parent(props);

        this._content_grid.add(this._title_label);

        this.get_style_context().add_class(EosKnowledge.STYLE_CLASS_SECTION_PAGE);
        this._title_label.get_style_context().add_class(EosKnowledge.STYLE_CLASS_SECTION_PAGE_TITLE);
        this.add(this._content_grid);
        this.show_all();
    },

    set title (v) {
        if (this._title === v) return;
        this._title = v;
        this._title_label.label = this._title;
        this.notify('title');
    },

    get title () {
        if (this._title)
            return this._title;
        return '';
    },

    set segments (v) {
        if (this._cards_map === v)
            return;

        if (this._segments !== null) {
            for (let segment of this._segments) {
               this._content_grid.remove(segment);
            }
        }

        this._cards_map = v;
        this._segments = [];
        if (this._cards_map !== null) {
            for (let segment_title in this._cards_map) {
                let cards = this._cards_map[segment_title];

                let segment = new CardsSegment({
                    title: segment_title
                });

                segment.cards = cards;
                this._segments.push(segment);
                this._content_grid.add(segment);
                segment.show_all();
            }
        }
    },

    get segments () {
        return this._cards_map;
    }
});

const CardsSegment = new Lang.Class({
    Name: 'CardsSegment',
    GTypeName: 'EknCardsSegment',
    Extends: Gtk.Grid,

    Properties: {
        // title property
        // A string with the title of the segment. Defaults to an empty string.
        'title': GObject.ParamSpec.string('title', 'Segment Title',
            'Title of the segment',
            GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE, '')

    },

    _init: function (props) {
        props = props || {};
        props.column_spacing = 20;
        props.row_spacing = 20;
        props.expand = true;

        this._title_label = new Gtk.Label({
            halign: Gtk.Align.START,
            valign: Gtk.Align.START,
            wrap: true,
            max_width_chars: 8 // TODO: change this after clarifying with design how resizing should work
        });

        let separator = new Gtk.Separator({
            expand: true,
            halign: Gtk.Align.FILL
        });

        this._flexi_grid = new Gtk.Grid({
            expand: true,
            halign: Gtk.Align.START
        });

        this._cards = null;

        this.parent(props);

        this.attach(separator, 0, 0, 2, 1);
        this.attach(this._title_label, 0, 1, 1, 1);
        this.attach(this._flexi_grid, 1, 1, 1, 1);

        this._title_label.get_style_context().add_class(EosKnowledge.STYLE_CLASS_SECTION_PAGE_SEGMENT_TITLE);

    },

    get title () {
        if (this._title)
            return this._title;
        return '';
    },

    set title (v) {
        if (this._title === v) return;
        this._title = v;
        this._title_label.label = this._title.toUpperCase();
        this.notify('title');
    },

    set cards (v) {
        if (this._cards === v)
            return;
        if (this._cards !== null) {
            for (let card of this._cards) {
                this._flexi_grid.remove(card);
            }
        }

        this._cards = v;
        if (this._cards !== null) {
            for (let card of this._cards) {
                this._flexi_grid.add(card);
            }
        }
    },

    get cards () {
        return this._cards;
    }
});
