// Copyright 2016 Endless Mobile, Inc.

const Eknc = imports.gi.EosKnowledgeContent;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;

Gtk.init(null);

const Utils = imports.tests.utils;
Utils.register_gresource();

const Course = imports.app.modules.controller.course;
const HistoryStore = imports.app.historyStore;
const Module = imports.app.interfaces.module;
const MockEngine = imports.tests.mockEngine;
const MockFactory = imports.tests.mockFactory;

const MockView = new Module.Class({
    Name: 'testCourseController_MockView',
    Extends: Gtk.Window,
    Implements: [ Module.Module ],

    Properties: {
        'template-type': GObject.ParamSpec.string('template-type', '', '',
            GObject.ParamFlags.READWRITE, ''),
    },
});

describe('Controller.Course', function () {
    let course, engine, factory, set_models, store;

    beforeEach(function () {

        set_models = [0, 1, 2].map(() => Eknc.SetObjectModel.new_from_props({
            tags: ['foo'],
        }));
        let parent = Eknc.SetObjectModel.new_from_props({
            child_tags: ['foo'],
        });
        set_models.push(parent);

        engine = MockEngine.mock_default();
        engine.query_promise.and.returnValue(Promise.resolve({ models: set_models }));

        [course, factory] = MockFactory.setup_tree({
            type: Course.Course,
            properties: {
                'theme': '',
            },
            slots: {
                'window': { type: MockView },
            },
        });
        store = HistoryStore.get_default();
    });
});
