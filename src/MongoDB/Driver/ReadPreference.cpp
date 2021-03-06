/**
 *  Copyright 2014-2015 MongoDB, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "hphp/runtime/ext/extension.h"
#include "hphp/runtime/vm/native-data.h"
#include "hphp/runtime/base/array-iterator.h"

#undef TRACE

#include "../../../mongodb.h"
#include "../../../utils.h"

#include "ReadPreference.h"

namespace HPHP {

const StaticString s_MongoDriverReadPreference_className("MongoDB\\Driver\\ReadPreference");
Class* MongoDBDriverReadPreferenceData::s_class = nullptr;
const StaticString MongoDBDriverReadPreferenceData::s_className("MongoDBDriverReadPreference");
IMPLEMENT_GET_CLASS(MongoDBDriverReadPreferenceData);

bool hippo_mongo_driver_readpreference_are_valid(const Variant tags)
{
	if (!tags.isArray()) {
		return false;
	}

	for (ArrayIter iter(tags.toArray()); iter; ++iter) {
		const Variant& data(iter.secondRef());

		if (!data.isArray() && !data.isObject()) {
			return false;
		}
	}

	return true;
}

Array hippo_mongo_driver_readpreference_prep_tagsets(const Array &tags)
{
	Array newTags = Array::Create();

	for (ArrayIter iter(tags); iter; ++iter) {
		const Variant& key = iter.first();
		const Variant& data(iter.secondRef());

		if (data.isArray()) {
			newTags.add(key, data.toObject());
		} else {
			newTags.add(key, data);
		}
	}

	return newTags;
}

void HHVM_METHOD(MongoDBDriverReadPreference, _setReadPreference, int readPreference)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);

	data->m_read_preference = mongoc_read_prefs_new((mongoc_read_mode_t) readPreference);
}

void HHVM_METHOD(MongoDBDriverReadPreference, _setReadPreferenceTags, const Array &tagSets)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);
	bson_t *bson;

	/* Check validity */
	if (!hippo_mongo_driver_readpreference_are_valid(tagSets)) {
		throw MongoDriver::Utils::throwInvalidArgumentException("tagSets must be an array of zero or more documents");
	}

	/* Validate that readPreferenceTags are not used with PRIMARY readPreference */
	if (mongoc_read_prefs_get_mode(data->m_read_preference) == MONGOC_READ_PRIMARY) {
		throw MongoDriver::Utils::throwInvalidArgumentException("tagSets may not be used with primary mode");
	}

	/* Convert argument */
	VariantToBsonConverter converter(tagSets, HIPPO_BSON_NO_FLAGS);
	bson = bson_new();
	converter.convert(bson);

	/* Set and check errors */
	mongoc_read_prefs_set_tags(data->m_read_preference, bson);
	bson_destroy(bson);
	if (!mongoc_read_prefs_is_valid(data->m_read_preference)) {
		/* Throw exception */
		throw MongoDriver::Utils::throwInvalidArgumentException("Read preference is not valid");
	}
}

void HHVM_METHOD(MongoDBDriverReadPreference, _setMaxStalenessSeconds, int maxStalenessSeconds)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);

	/* Validate that maxStalenessSeconds is not used with PRIMARY readPreference */
	if (mongoc_read_prefs_get_mode(data->m_read_preference) == MONGOC_READ_PRIMARY && maxStalenessSeconds != MONGOC_NO_MAX_STALENESS) {
		throw MongoDriver::Utils::throwInvalidArgumentException("maxStalenessSeconds may not be used with primary mode");
	}

	mongoc_read_prefs_set_max_staleness_seconds(data->m_read_preference, maxStalenessSeconds);

	if (!mongoc_read_prefs_is_valid(data->m_read_preference)) {
		/* Throw exception */
		throw MongoDriver::Utils::throwInvalidArgumentException("Read preference is not valid");
	}
}

const StaticString
	s_mode("mode"),
	s_tags("tags"),
	s_maxStalenessSeconds("maxStalenessSeconds");


Array HHVM_METHOD(MongoDBDriverReadPreference, __debugInfo)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);
	Array retval = Array::Create();
	Variant v_tags;
	const bson_t *tags = mongoc_read_prefs_get_tags(data->m_read_preference);

	mongoc_read_mode_t mode = mongoc_read_prefs_get_mode(data->m_read_preference);

	switch (mode) {
		case MONGOC_READ_PRIMARY: retval.set(s_mode, "primary"); break;
		case MONGOC_READ_PRIMARY_PREFERRED: retval.set(s_mode, "primaryPreferred"); break;
		case MONGOC_READ_SECONDARY: retval.set(s_mode, "secondary"); break;
		case MONGOC_READ_SECONDARY_PREFERRED: retval.set(s_mode, "secondaryPreferred"); break;
		case MONGOC_READ_NEAREST: retval.set(s_mode, "nearest"); break;
		default: /* Do nothing */
			break;
	}

	if (!bson_empty(tags)) {
		hippo_bson_conversion_options_t options = HIPPO_TYPEMAP_INITIALIZER;
		BsonToVariantConverter convertor(bson_get_data(tags), tags->len, options);
		convertor.convert(&v_tags);
		retval.set(s_tags, v_tags.toArray());
	}

	if (mongoc_read_prefs_get_max_staleness_seconds(data->m_read_preference) != MONGOC_NO_MAX_STALENESS) {
		retval.set(s_maxStalenessSeconds, mongoc_read_prefs_get_max_staleness_seconds(data->m_read_preference));
	}

	return retval;
}

Variant HHVM_METHOD(MongoDBDriverReadPreference, bsonSerialize)
{
	Array retval = HHVM_MN(MongoDBDriverReadPreference, __debugInfo)(this_);
	return Variant(Variant(retval).toObject());
}

int64_t HHVM_METHOD(MongoDBDriverReadPreference, getMode)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);

	return mongoc_read_prefs_get_mode(data->m_read_preference);
}

Array HHVM_METHOD(MongoDBDriverReadPreference, getTagSets)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);
	Variant v_tags;
	const bson_t *tags = mongoc_read_prefs_get_tags(data->m_read_preference);
	
	hippo_bson_conversion_options_t options = HIPPO_TYPEMAP_DEBUG_INITIALIZER;
	BsonToVariantConverter convertor(bson_get_data(tags), tags->len, options);
	convertor.convert(&v_tags);

	return v_tags.toArray();
}

int64_t HHVM_METHOD(MongoDBDriverReadPreference, getMaxStalenessSeconds)
{
	MongoDBDriverReadPreferenceData* data = Native::data<MongoDBDriverReadPreferenceData>(this_);

	return mongoc_read_prefs_get_max_staleness_seconds(data->m_read_preference);
}

}
