// Copyright 2015 Endless Mobile, Inc.

const Lang = imports.lang;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;

Gtk.init(null);

const Utils = imports.tests.utils;
Utils.register_gresource();

const Actions = imports.app.actions;
const ArticleObjectModel = imports.search.articleObjectModel;
const BuffetInteraction = imports.app.modules.buffetInteraction;
const ContentObjectModel = imports.search.contentObjectModel;
const Launcher = imports.app.interfaces.launcher;
const Module = imports.app.interfaces.module;
const MockDispatcher = imports.tests.mockDispatcher;
const MockEngine = imports.tests.mockEngine;
const MockFactory = imports.tests.mockFactory;
const SetObjectModel = imports.search.setObjectModel;

const MockView = new Lang.Class({
    Name: 'testBuffetInteraction_MockView',
    Extends: Gtk.Window,
    Implements: [ Module.Module ],

    Properties: {
        'factory': GObject.ParamSpec.override('factory', Module.Module),
        'factory-name': GObject.ParamSpec.override('factory-name', Module.Module),
        'template-type': GObject.ParamSpec.string('template-type', '', '',
            GObject.ParamFlags.READWRITE, ''),
    },
});

describe('Buffet interaction', function () {
    let buffet, dispatcher, engine, factory, set_models, article_models;

    beforeEach(function () {
        dispatcher = MockDispatcher.mock_default();

        set_models = [0, 1, 2].map(() => new SetObjectModel.SetObjectModel());
        article_models = [0, 1, 2, 3, 4, 5].map(() =>
            new ArticleObjectModel.ArticleObjectModel());

        // Return different models based on what the interaction is asking for
        engine = MockEngine.mock_default();
        engine.get_objects_by_query.and.callFake((query, cancel, callback) => {
            let task = query.tags.indexOf('EknSetObject') > -1 ? 'sets' : 'articles';
            callback(engine, task);
            return task;
        });
        engine.get_objects_by_query_finish.and.callFake(task => {
            if (task === 'sets')
                return [set_models, null];
            return [article_models, null];
        });

        factory = new MockFactory.MockFactory();
        factory.add_named_mock('window', MockView);
        factory.add_named_mock('interaction', BuffetInteraction.BuffetInteraction, {
            'window': 'window',
        });

        buffet = new BuffetInteraction.BuffetInteraction({
            factory: factory,
            factory_name: 'interaction',
        });
    });

    it('dispatches app-launched on launch from desktop', function () {
        buffet.desktop_launch(0);
        expect(dispatcher.last_payload_with_type(Actions.FIRST_LAUNCH).launch_type)
            .toBe(Launcher.LaunchType.DESKTOP);
    });

    it('dispatches app-launched on launch from search', function () {
        buffet.search(0, 'query');
        expect(dispatcher.last_payload_with_type(Actions.FIRST_LAUNCH).launch_type)
            .toBe(Launcher.LaunchType.SEARCH);
    });

    it('dispatches app-launched on launch from search result', function () {
        engine.get_object_by_id_finish.and.returnValue(new ContentObjectModel.ContentObjectModel());
        buffet.activate_search_result(0, 'ekn://foo/bar', 'query');
        expect(dispatcher.last_payload_with_type(Actions.FIRST_LAUNCH).launch_type)
            .toBe(Launcher.LaunchType.SEARCH_RESULT);
    });

    it('dispatches app-launched only once', function () {
        engine.get_object_by_id_finish.and.returnValue(new ContentObjectModel.ContentObjectModel());

        buffet.desktop_launch(0);
        let payloads = dispatcher.payloads_with_type(Actions.FIRST_LAUNCH);
        expect(payloads.length).toBe(1);

        buffet.desktop_launch(0);
        buffet.search(0, 'query');
        buffet.activate_search_result(0, 'ekn://foo/bar', 'query');

        payloads = dispatcher.payloads_with_type(Actions.FIRST_LAUNCH);
        expect(payloads.length).toBe(1);
    });

    it('indicates that the brand screen has been read after launch from desktop', function () {
        buffet.BRAND_SCREEN_TIME_MS = 0;
        buffet.desktop_launch(0);
        expect(dispatcher.last_payload_with_type(Actions.BRAND_SCREEN_DONE)).not.toBeDefined();
        Utils.update_gui();
        expect(dispatcher.last_payload_with_type(Actions.BRAND_SCREEN_DONE)).toBeDefined();
    });

    it('shows the brand screen only once', function () {
        buffet.BRAND_SCREEN_TIME_MS = 0;
        buffet.desktop_launch(0);
        buffet.desktop_launch(0);
        Utils.update_gui();
        let payloads = dispatcher.payloads_with_type(Actions.BRAND_SCREEN_DONE);
        expect(payloads.length).toBe(1);
    });

    it('does not show the brand screen on other launch methods', function () {
        buffet.BRAND_SCREEN_TIME_MS = 0;
        engine.get_object_by_id_finish.and.returnValue(new ContentObjectModel.ContentObjectModel());
        buffet.search(0, 'query');
        buffet.activate_search_result(0, 'ekn://foo/bar', 'query');
        Utils.update_gui();
        expect(dispatcher.last_payload_with_type(Actions.BRAND_SCREEN_DONE)).not.toBeDefined();
    });

    it('dispatches set models to populate the app with', function () {
        let payloads = dispatcher.payloads_with_type(Actions.APPEND_SETS);
        expect(payloads.length).toBe(1);
        expect(set_models).toEqual(payloads[0].models);
        expect(engine.get_objects_by_query)
            .toHaveBeenCalledWith(jasmine.objectContaining({ tags: ['EknSetObject'] }),
                jasmine.any(Object), jasmine.any(Function));
    });

    it('dispatches article models to populate the app with', function () {
        let payloads = dispatcher.payloads_with_type(Actions.APPEND_ITEMS);
        expect(payloads.length).toBe(1);
        expect(article_models).toEqual(payloads[0].models);
        expect(engine.get_objects_by_query)
            .toHaveBeenCalledWith(jasmine.objectContaining({ tags: ['EknArticleObject'] }),
                jasmine.any(Object), jasmine.any(Function));
    });
});