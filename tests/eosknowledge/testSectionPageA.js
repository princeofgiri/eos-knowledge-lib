const EosKnowledge = imports.gi.EosKnowledge;
const Endless = imports.gi.Endless;
const Gtk = imports.gi.Gtk;

const CssClassMatcher = imports.CssClassMatcher;
const WidgetDescendantMatcher = imports.WidgetDescendantMatcher;

const TESTDIR = Endless.getCurrentFileDir() + '/..';

EosKnowledge.init();

let segments = {

    'Lessons': [
        new EosKnowledge.Card({
            title: 'Subtitled Card',
            synopsis: 'This is the Subtitle',
        }),
        new EosKnowledge.Card({
            title: 'Picture Card',
            thumbnail_uri: TESTDIR + '/test-content/pig1.jpg',
        })
    ],

    'Articles': [
        new EosKnowledge.Card({
            title: 'Everything card',
            synopsis: 'This card has everything',
            thumbnail_uri: TESTDIR + '/test-content/pig2.jpg',
        }),
        new EosKnowledge.LessonCard({
            title: 'Mustard lesson',
            synopsis: 'Sample, incomplete',
            // By Bogdan29roman, CC-BY-SA
            // http://en.wikipedia.org/wiki/File:Mu%C5%9Ftar.jpg
            thumbnail_uri: TESTDIR + '/test-content/mustard.jpg',
            item_index: 1,
            complete: false
        })
    ]
};

describe('Section page for Template A', function () {
    let section_page;

    beforeEach(function () {
        jasmine.addMatchers(CssClassMatcher.customMatchers);
        jasmine.addMatchers(WidgetDescendantMatcher.customMatchers);

        section_page = new EosKnowledge.SectionPageA({
            title: "History of Guatemala"
        });

        notify = jasmine.createSpy('notify');
        section_page.connect('notify', function (object, pspec) {
            // Seems properties defined in js can only be accessed through
            // object[name] with the underscore variant on the name
            notify(pspec.name, object[pspec.name.replace('-', '_')]);
        });

    });

    it('can be constructed', function () {});

    it('can set cards', function () {
        for (let segment_title in segments) {
            section_page.append_to_segment(segment_title, segments[segment_title]);
            for (let card of segments[segment_title]) {
                expect(section_page).toHaveDescendant(card);
            }
        }
    });

    it('can set title', function () {
        section_page.title = "Brazil";
        expect(section_page.title).toBe("Brazil");
    });

    describe('Style class of section page', function () {
        it('has section-page-a class', function () {
            expect(section_page).toHaveCssClass(EosKnowledge.STYLE_CLASS_SECTION_PAGE_A);
        });

        it('has a descendant with title class', function () {
            expect(section_page).toHaveDescendantWithCssClass(EosKnowledge.STYLE_CLASS_SECTION_PAGE_TITLE);
        });

        it ('has a descendant with no_search_results class', function () {
            expect(section_page).toHaveDescendantWithCssClass(EosKnowledge.STYLE_CLASS_SECTION_PAGE_NO_SEARCH_RESULTS);
        });

        it('has a descendant with segment_title class', function () {
            for (let segment_title in segments) {
                section_page.append_to_segment(segment_title, segments[segment_title]);
            }
            expect(section_page).toHaveDescendantWithCssClass(EosKnowledge.STYLE_CLASS_SECTION_PAGE_A_SEGMENT_TITLE);
        });

    });
});
