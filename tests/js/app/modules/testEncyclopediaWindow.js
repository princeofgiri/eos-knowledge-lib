// Copyright 2016 Endless Mobile, Inc.

const Endless = imports.gi.Endless;
const GLib = imports.gi.GLib;

const Minimal = imports.tests.minimal;
const MockFactory = imports.tests.mockFactory;
const EncyclopediaWindow = imports.app.modules.encyclopediaWindow;

describe('Encyclopedia Window', function () {
    let view, app, factory;

    beforeAll(function (done) {
        // Generate a unique ID for each app instance that we test
        let fake_pid = GLib.random_int();
        // FIXME In this version of GJS there is no Posix module, so fake the PID
        let id_string = 'com.endlessm.knowledge.test.dummy' + GLib.get_real_time() + fake_pid;
        app = new Endless.Application({
            application_id: id_string,
            flags: 0
        });
        app.connect('startup', done);
        app.hold();
        app.run([]);
    });

    afterAll(function () {
        app.release();
    });

    beforeEach(function () {
        factory = new MockFactory.MockFactory();
        factory.add_named_mock('home-page', Minimal.MinimalPage);
        factory.add_named_mock('search-page', Minimal.MinimalPage);
        factory.add_named_mock('lightbox', Minimal.MinimalBinModule);
        factory.add_named_mock('window', EncyclopediaWindow.EncyclopediaWindow, {
            'home-page': 'home-page',
            'search-page': 'search-page',
            'lightbox': 'lightbox',
        });

        view = new EncyclopediaWindow.EncyclopediaWindow({
            application: app,
            factory: factory,
            factory_name: 'window',
        });
    });

    afterEach(function () {
        view.destroy();
    });

    it('can be constructed', function () {
        expect(view).toBeDefined();
    });

    it('disables the home button when in the home page', function () {
        let home_page = factory.get_created_named_mocks('home-page')[0];
        let other_page = factory.get_created_named_mocks('search-page')[0];
        expect(view._home_button).toBeDefined();
        view.show_page(other_page);
        expect(view._home_button.sensitive).toBe(true);
        view.show_page(home_page);
        expect(view._home_button.sensitive).toBe(false);
    });
});