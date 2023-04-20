// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "proc/formats-converter.h"
#include "stream.h"

namespace librealsense
{

void formats_converter::register_processing_block( const std::vector< stream_profile > & from,
                                                    const std::vector< stream_profile > & to,
                                                    std::function< std::shared_ptr< processing_block >( void ) > generate_func )
{
    _pb_factories.push_back( std::make_shared< processing_block_factory >( from, to, generate_func ) );
}


void formats_converter::register_processing_block( const processing_block_factory & pbf )
{
    _pb_factories.push_back( std::make_shared<processing_block_factory>( pbf ) );
}

void formats_converter::register_processing_block( const std::vector< processing_block_factory > & pbfs )
{
    for( auto && pbf : pbfs )
        register_processing_block( pbf );
}

stream_profiles formats_converter::get_all_possible_target_profiles( const stream_profiles & from_profiles )
{
    // For each profile that can be used as input (from_profiles) check all registered factories if they can create
    // a converter from the profile type (source). If so, create appropriate profiles for all possible target types
    // that the converters support.
    // Note - User profile type is stream_profile_interface, factories profile type is stream_profile.
    stream_profiles to_profiles;

    for( auto & from_profile : from_profiles )
    {
        for( auto && pbf : _pb_factories )
        {
            const auto && sources = pbf->get_source_info();
            for( auto && source : sources )
            {
                if( from_profile->get_format() == source.format &&
                   ( source.stream == from_profile->get_stream_type() || source.stream == RS2_STREAM_ANY ) )
                {
                    const auto & targets = pbf->get_target_info();
                    // targets are saved with format and type only. Updating fps and resolution before using as key
                    for( auto target : targets )
                    {

                        target.fps = from_profile->get_framerate();

                        auto && cloned_profile = clone_profile( from_profile );
                        cloned_profile->set_format( target.format );
                        cloned_profile->set_stream_index( target.index ); //TODO - shouldn't be from_profile.index?
                        cloned_profile->set_stream_type( target.stream );

                        auto && cloned_vsp = As< video_stream_profile, stream_profile_interface >( cloned_profile );
                        if( cloned_vsp )
                        {
                            target.height = cloned_vsp->get_height();
                            target.width = cloned_vsp->get_width();
                        }

                        // Cache pbf supported profiles for efficiency in find_pbf_matching_most_profiles
                        _pbf_supported_profiles[pbf.get()].push_back( cloned_profile );

                        // Cache each target profile to its source profiles which were generated from.
                        _target_to_source_profiles_map[target].push_back( from_profile );

                        // Duplicates in the list happen when 2 from_profiles have conversion to same target.
                        // In this case it is faster to check if( _target_to_source_profiles_map[target].size() > 1 )
                        // rather then if( is_profile_in_list( cloned_profile, to_profiles ) )
                        if( _target_to_source_profiles_map[target].size() > 1 )
                            continue;

                        // Only injective cloning in many to one mapping.
                        // One to many is not affected.
                        if( sources.size() > 1 && target.format != source.format )
                            continue;

                        to_profiles.push_back( cloned_profile );
                    }
                }
            }
        }
    }

    return to_profiles;
}

std::shared_ptr< stream_profile_interface > formats_converter::clone_profile( const std::shared_ptr< stream_profile_interface > & profile ) const
{
    std::shared_ptr< stream_profile_interface > cloned = nullptr;

    if( auto vsp = std::dynamic_pointer_cast< video_stream_profile >( profile ) )
    {
        cloned = vsp->clone();
    }
    else if( auto msp = std::dynamic_pointer_cast< motion_stream_profile >( profile ) )
    {
        cloned = msp->clone();
    }
    else
        throw librealsense::not_implemented_exception( "Unsupported profile type to clone" );

    if( !cloned )
        throw librealsense::invalid_value_exception( "failed to clone profile" );

    // No need to set the ID, when calling get_all_possible_target_profiles the from_profiles don't have an assigned ID
    // yet, it will be assigned according to stream later on.
    // cloned->set_unique_id( profile->get_unique_id() ); // clone() issues a new id for the profile, we want to use old one
    // 
    // Needed for clone_profile wholeness but will be overwritten later by target profile fields. Can skip.
    // cloned->set_format( profile->get_format() );
    // cloned->set_stream_index( profile->get_stream_index() );
    // cloned->set_stream_type( profile->get_stream_type() );

    return cloned;
}

bool formats_converter::is_profile_in_list( const std::shared_ptr< stream_profile_interface > & profile, const stream_profiles & profiles ) const
{
    // Converting to stream_profile to avoid dynamic casting to video/motion_stream_profile
    const auto && is_duplicate_predicate = [&profile]( const std::shared_ptr< stream_profile_interface > & spi ) {
        return to_profile( spi.get() ) == to_profile( profile.get() );
    };

    return std::any_of( begin( profiles ), end( profiles ), is_duplicate_predicate );
}

//std::vector< processing_block > formats_converter::get_converters( std::vector< stream_profile > ) const
//{
//
//}

stream_profiles formats_converter::get_source_of_profiles( stream_profiles target_profiles )
{
    std::unordered_set<std::shared_ptr<stream_profile_interface>> resolved_req_set;
    stream_profiles resolved_req;

    // Add missing data to source profiles (was not available during get_all_possible_target_profiles)
    for( auto & target_profile : target_profiles )
    {
        //TODO - original code only updates source[0], why?
        for( auto & source_profile : _target_to_source_profiles_map[to_profile( target_profile.get() )] )
        {
            source_profile->set_unique_id( target_profile->get_unique_id() );
            source_profile->set_stream_index( target_profile->get_stream_index() );
            source_profile->set_unique_id( target_profile->get_unique_id() );
            source_profile->set_stream_type( target_profile->get_stream_type() );
            auto & source_video_profile = As<video_stream_profile, stream_profile_interface>( source_profile );
            const auto & target_video_profile = As<video_stream_profile, stream_profile_interface>( target_profile );
            if( source_video_profile )
            {
                source_video_profile->set_intrinsics( [target_video_profile]() {
                    if( target_video_profile )
                        return target_video_profile->get_intrinsics();
                    else
                        return rs2_intrinsics {};
                } );
            }
        }
    }

    // cache the requests
    for( auto && target_profile : target_profiles )
    {
        // Caching target profiles to set as processed frames profile before calling user callback
        _format_to_target_profiles[target_profile->get_format()].push_back( target_profile );
    }

    // while not finished handling all of the requests do
    while( ! target_profiles.empty() )
    {
        const auto & best_match = find_pbf_matching_most_profiles( target_profiles );
        auto & best_match_pbf = best_match.first;
        auto & best_match_target_profiles = best_match.second;

        // mark as handled resolved requests
        for( auto & profile : best_match_target_profiles )
        {
            const auto & matching_profiles_predicate = [&profile]( const std::shared_ptr<stream_profile_interface> & sp ) {
                return to_profile( profile.get() ) == to_profile( sp.get() );
            };
            target_profiles.erase( std::remove_if( begin( target_profiles ), end( target_profiles ), matching_profiles_predicate ) );
        }

        // Retrieve source profile from cached map and generate the relevant processing block.
        std::unordered_set<std::shared_ptr<stream_profile_interface>> current_resolved_reqs;
        auto best_pb = best_match_pbf->generate();
        //register_processing_block_options( *best_pb );
        for( const auto & target_profile : best_match_target_profiles )
        {
            auto & mapped_source_profiles = _target_to_source_profiles_map[to_profile( target_profile.get() )];

            for( const auto & source_profile : mapped_source_profiles )
            {
                if( best_match_pbf->has_source( source_profile ) )
                {
                    resolved_req_set.insert( source_profile );
                    current_resolved_reqs.insert( source_profile );

                    // Caching processing blocks to set their callback during sensor::start
                    _source_profile_to_converters[source_profile].insert( best_pb );
                }
            }
        }
        const stream_profiles & print_current_resolved_reqs = { current_resolved_reqs.begin(), current_resolved_reqs.end() };
        LOG_INFO( "Request: " << best_match_target_profiles << "\nResolved to: " << print_current_resolved_reqs );
    }

    resolved_req = { resolved_req_set.begin(), resolved_req_set.end() };
    return resolved_req;
}

std::pair< std::shared_ptr< processing_block_factory >, stream_profiles >
formats_converter::find_pbf_matching_most_profiles( const stream_profiles & profiles )
{
    // Find and retrieve best fitting processing block to the given requests, and the requests which were the best fit.

    // For video stream, the best fitting processing block is defined as the processing block which its sources
    // covers the maximum amount of requests.

    stream_profiles best_match_profiles;
    std::shared_ptr<processing_block_factory> best_match_processing_block_factory;

    size_t best_source_size = 0;
    size_t max_satisfied_count = 0;

    for( auto & pbf : _pb_factories )
    {
        stream_profiles && satisfied_req = pbf->find_satisfied_requests( profiles, _pbf_supported_profiles[pbf.get()] );
        size_t satisfied_count = satisfied_req.size();
        size_t source_size = pbf->get_source_info().size();
        if( satisfied_count > max_satisfied_count ||
           ( satisfied_count == max_satisfied_count && source_size < best_source_size ) )
        {
            max_satisfied_count = satisfied_count;
            best_source_size = source_size;
            best_match_processing_block_factory = pbf;
            best_match_profiles = std::move( satisfied_req );
        }
    }

    return { best_match_processing_block_factory, best_match_profiles };
}

void formats_converter::clear_cached_source_list()
{
    _source_profile_to_converters.clear();
    _format_to_target_profiles.clear();
}

template<class T>
frame_callback_ptr make_callback( T callback )
{
    return {
        new internal_frame_callback<T>( callback ),
        []( rs2_frame_callback * p ) { p->release(); }
    };
}

void formats_converter::set_frames_callback( frame_callback_ptr callback )
{
    _converted_frames_callback = callback;

    // After processing callback
    const auto && output_cb = make_callback( [&]( frame_holder f ) {
        std::vector< frame_interface * > frames_to_be_processed;
        frames_to_be_processed.push_back( f.frame );

        const auto & composite = dynamic_cast< composite_frame * >( f.frame );
        if( composite )
        {
            for( size_t i = 0; i < composite->get_embedded_frames_count(); i++ )
            {
                frames_to_be_processed.push_back( composite->get_frame( static_cast< int >( i ) ) );
            }
        }

        // Process only frames which aren't composite.
        for( auto && fr : frames_to_be_processed )
        {
            if( !dynamic_cast< composite_frame * >( fr ) )
            {
                //TODO - find out why we need to find a target profile with the same format+index+type as the frame profile
                // and save if back to the frame. Different unique_id?
                // Reason - viewer uses syncher and matcher that uses rs2::stream_profile.clone() that generate a new ID
                // for the clone and than the match can fail.
                auto && cached_profile = filter_frame_by_requests( fr );

                if( cached_profile )
                {
                    fr->set_stream( cached_profile );
                }
                else
                    continue;

                fr->acquire();
                if( _converted_frames_callback )
                    _converted_frames_callback->on_frame( ( rs2_frame * )fr );
            }
        }
    } );

    // Set callbacks for all of the relevant processing blocks
    for( const auto & entry : _source_profile_to_converters )
    {
        auto & converters = entry.second;
        for( auto & converter : converters )
            if( converter )
            {
                converter->set_output_callback( output_cb );
            }
    }
}

void formats_converter::convert_frame( frame_holder & f )
{
    if( !f )
        return;

    auto & converters = _source_profile_to_converters[f->get_stream()];
    for( auto && converter : converters )
    {
        f->acquire();
        converter->invoke( f.frame );
    }
}

//////////////////
std::shared_ptr<stream_profile_interface> formats_converter::filter_frame_by_requests( const frame_interface * f )
{
    const auto & cached_req = _format_to_target_profiles.find( f->get_stream()->get_format() );
    if( cached_req == _format_to_target_profiles.end() )
        return nullptr;

    auto & reqs = cached_req->second;
    auto & req_it = std::find_if( begin( reqs ), end( reqs ), [&f]( const std::shared_ptr<stream_profile_interface> & req ) {
        return ( req->get_stream_index() == f->get_stream()->get_stream_index() &&
                 req->get_stream_type() == f->get_stream()->get_stream_type() );
        } );

    return req_it != end( reqs ) ? *req_it : nullptr;
}

} // namespace librealsense
