const Gtk = imports.gi.Gtk;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const Endless = imports.gi.Endless;
const EosKnowledge = imports.gi.EosKnowledge;
const Lang = imports.lang;

const TEST_APPLICATION_ID = 'com.endlessm.knowledge.card';
const TESTDIR = Endless.getCurrentFileDir() + '/..';

const TestApplication = new Lang.Class ({
    Name: 'TestApplication',
    Extends: Endless.Application,

    vfunc_startup: function() {
        this.parent();

        let provider = new Gtk.CssProvider();
        let css_file = Gio.File.new_for_uri('resource:///com/endlessm/knowledge/endless_knowledge.css');
        provider.load_from_file(css_file);
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),
                                                 provider,
                                                 Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

        let cards = [
            new EosKnowledge.Card({
                title: 'Subtitled Card',
                subtitle: 'This is the Subtitle',
            }),
            new EosKnowledge.Card({
                title: 'Picture Card',
                thumbnail_uri: TESTDIR + '/test-content/pig1.jpg',
            }),
            new EosKnowledge.Card({
                title: 'Everything card',
                subtitle: 'This card has everything',
                thumbnail_uri: TESTDIR + '/test-content/pig2.jpg',
            })
        ];
        let grid = new Gtk.Grid({
            orientation: Gtk.Orientation.HORIZONTAL
        });

        cards.forEach(function (card, index) {
            card.expand = true;
            card.valign = Gtk.Align.CENTER;
            card.halign = Gtk.Align.CENTER;
            card.connect('clicked', function () {
                print('Card', index + 1, 'clicked');
            });
            grid.add(card);
        });

        let window = new Endless.Window({
            application: this
        });
        window.get_page_manager().add(grid);
        window.show_all();
    }
});

let app = new TestApplication({
    application_id: TEST_APPLICATION_ID,
    flags: 0
});
app.run(ARGV);
