// Copyright 2016 Endless Mobile, Inc.

const Eknc = imports.gi.EosKnowledgeContent;
const GObject = imports.gi.GObject;
const Lang = imports.lang;

const Unread = imports.app.modules.filter.unread;
const MockFactory = imports.tests.mockFactory;
const MockReadingHistoryModel = imports.tests.mockReadingHistoryModel;

describe('Filter.Unread', function () {
    let filter, history_model, models;

    const IDS = ['read', 'unread'];

    beforeEach(function () {
        models = IDS.map(ekn_id =>
            Eknc.ContentObjectModel.new_from_props({
                ekn_id: ekn_id,
            }));
        history_model = MockReadingHistoryModel.mock_default();
        history_model.mark_article_read('read');
    });

    describe('normal mode', function () {
        beforeEach(function () {
            [filter] = MockFactory.setup_tree({
                type: Unread.Unread,
            });
        });

        it('is the default', function () {
            expect(filter.invert).toBeFalsy();
        });

        it('filters out a read card', function () {
            expect(filter.include(models[0])).toBeFalsy();
            expect(filter.include(models[1])).toBeTruthy();
        });

        it('does not query read IDs', function () {
            let query = filter.modify_xapian_query(new Eknc.QueryObject());
            printerr('returned excluded ids =', query.excluded_ids);
            expect(query.excluded_ids).toContain('read');
        });
    });

    describe('inverse mode', function () {
        beforeEach(function () {
            [filter] = MockFactory.setup_tree({
                type: Unread.Unread,
                properties: {
                    invert: true,
                }
            });
        });

        it('filters out an unread card', function () {
            expect(filter.include(models[0])).toBeTruthy();
            expect(filter.include(models[1])).toBeFalsy();
        });

        it('queries only read IDs', function () {
            let query = filter.modify_xapian_query(new Eknc.QueryObject());
            expect(query.ids).toContain('read');
        });
    });
});
