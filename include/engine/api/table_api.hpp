#ifndef ENGINE_API_TABLE_HPP
#define ENGINE_API_TABLE_HPP

#include "engine/api/base_api.hpp"
#include "engine/api/json_factory.hpp"
#include "engine/api/table_parameters.hpp"

#include "engine/datafacade/datafacade_base.hpp"

#include "engine/guidance/assemble_geometry.hpp"
#include "engine/guidance/assemble_leg.hpp"
#include "engine/guidance/assemble_overview.hpp"
#include "engine/guidance/assemble_route.hpp"
#include "engine/guidance/assemble_steps.hpp"

#include "engine/internal_route_result.hpp"

#include "util/integer_range.hpp"

#include <boost/range/algorithm/transform.hpp>

#include <iterator>

namespace osrm
{
namespace engine
{
namespace api
{

class TableAPI final : public BaseAPI
{
  public:
    struct TableCellRef
    {
        TableCellRef(const std::size_t &row, const std::size_t &column) : row{row}, column{column}
        {
        }
        std::size_t row;
        std::size_t column;
    };

    TableAPI(const datafacade::BaseDataFacade &facade_, const TableParameters &parameters_)
        : BaseAPI(facade_, parameters_), parameters(parameters_)
    {
    }

    virtual void
    MakeResponse(const std::pair<std::vector<EdgeDuration>, std::vector<EdgeDistance>> &tables,
                 const std::vector<PhantomNode> &phantoms,
                 const std::vector<TableCellRef> &fallback_speed_cells,
                 osrm::engine::api::ResultT &response) const
    {
        if(response.is<flatbuffers::FlatBufferBuilder>()) {
            auto& fb_result = response.get<flatbuffers::FlatBufferBuilder>();
            MakeResponse(tables, phantoms, fallback_speed_cells, fb_result);
        } else {
            auto& json_result = response.get<util::json::Object>();
            MakeResponse(tables, phantoms, fallback_speed_cells, json_result);
        }

    }

    virtual void
    MakeResponse(const std::pair<std::vector<EdgeDuration>, std::vector<EdgeDistance>> &tables,
                 const std::vector<PhantomNode> &phantoms,
                 const std::vector<TableCellRef> &fallback_speed_cells,
                 flatbuffers::FlatBufferBuilder &fb_result) const {
        auto number_of_sources = parameters.sources.size();
        auto number_of_destinations = parameters.destinations.size();

        fbresult::FBResultBuilder response(fb_result);
        response.add_code(fb_result.CreateString("Ok"));
        response.add_response_type(osrm::engine::api::fbresult::ServiceResponse::ServiceResponse_table);

        fbresult::TableBuilder table(fb_result);

        // symmetric case
        if (parameters.sources.empty())
        {
            table.add_sources(MakeWaypoints(fb_result, phantoms));
            number_of_sources = phantoms.size();
        }
        else
        {
            table.add_sources(MakeWaypoints(fb_result, phantoms, parameters.sources));
        }

        if (parameters.destinations.empty())
        {
            table.add_destinations(MakeWaypoints(fb_result, phantoms));
            number_of_destinations = phantoms.size();
        }
        else
        {
            table.add_destinations(MakeWaypoints(fb_result, phantoms, parameters.destinations));
        }

        if (parameters.annotations & TableParameters::AnnotationsType::Duration)
        {
            table.add_durations(MakeDurationTable(fb_result, tables.first, number_of_sources, number_of_destinations));
        }

        if (parameters.annotations & TableParameters::AnnotationsType::Distance)
        {
            table.add_distances(MakeDistanceTable(fb_result, tables.second, number_of_sources, number_of_destinations));
        }

        if (parameters.fallback_speed != INVALID_FALLBACK_SPEED && parameters.fallback_speed > 0)
        {
            table.add_fallback_speed_cells(MakeEstimatesTable(fb_result, fallback_speed_cells));
        }

        fb_result.Finish(response.Finish());
    }

    virtual void
    MakeResponse(const std::pair<std::vector<EdgeDuration>, std::vector<EdgeDistance>> &tables,
                 const std::vector<PhantomNode> &phantoms,
                 const std::vector<TableCellRef> &fallback_speed_cells,
                 util::json::Object &response) const
    {
        auto number_of_sources = parameters.sources.size();
        auto number_of_destinations = parameters.destinations.size();

        // symmetric case
        if (parameters.sources.empty())
        {
            response.values["sources"] = MakeWaypoints(phantoms);
            number_of_sources = phantoms.size();
        }
        else
        {
            response.values["sources"] = MakeWaypoints(phantoms, parameters.sources);
        }

        if (parameters.destinations.empty())
        {
            response.values["destinations"] = MakeWaypoints(phantoms);
            number_of_destinations = phantoms.size();
        }
        else
        {
            response.values["destinations"] = MakeWaypoints(phantoms, parameters.destinations);
        }

        if (parameters.annotations & TableParameters::AnnotationsType::Duration)
        {
            response.values["durations"] =
                MakeDurationTable(tables.first, number_of_sources, number_of_destinations);
        }

        if (parameters.annotations & TableParameters::AnnotationsType::Distance)
        {
            response.values["distances"] =
                MakeDistanceTable(tables.second, number_of_sources, number_of_destinations);
        }

        if (parameters.fallback_speed != INVALID_FALLBACK_SPEED && parameters.fallback_speed > 0)
        {
            response.values["fallback_speed_cells"] = MakeEstimatesTable(fallback_speed_cells);
        }

        response.values["code"] = "Ok";
    }

  protected:
    virtual flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::Waypoint>>>
    MakeWaypoints(flatbuffers::FlatBufferBuilder& builder, const std::vector<PhantomNode> &phantoms) const
    {
        std::vector<flatbuffers::Offset<fbresult::Waypoint>> waypoints;
        waypoints.reserve(phantoms.size());
        BOOST_ASSERT(phantoms.size() == parameters.coordinates.size());

        boost::range::transform(
                phantoms,
                std::back_inserter(waypoints),
                [this, &builder](const PhantomNode &phantom) { return BaseAPI::MakeWaypoint(builder, phantom); });
        return builder.CreateVector(waypoints);
    }

    virtual flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::Waypoint>>>
    MakeWaypoints(flatbuffers::FlatBufferBuilder& builder,
                  const std::vector<PhantomNode> &phantoms,
                  const std::vector<std::size_t> &indices) const
    {
        std::vector<flatbuffers::Offset<fbresult::Waypoint>> waypoints;
        waypoints.reserve(indices.size());
        boost::range::transform(indices,
                                std::back_inserter(waypoints),
                                [this, &builder, phantoms](const std::size_t idx) {
                                    BOOST_ASSERT(idx < phantoms.size());
                                    return BaseAPI::MakeWaypoint(builder, phantoms[idx]);
                                });
        return builder.CreateVector(waypoints);
    }

    virtual flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::VectorDouble>>>
    MakeDurationTable(flatbuffers::FlatBufferBuilder& builder,
                      const std::vector<EdgeWeight> &values,
                      std::size_t number_of_rows,
                      std::size_t number_of_columns) const
    {
        std::vector<flatbuffers::Offset<fbresult::VectorDouble>> fb_table;
        for (const auto row : util::irange<std::size_t>(0UL, number_of_rows))
        {
            std::vector<double> fb_row;
            auto row_begin_iterator = values.begin() + (row * number_of_columns);
            auto row_end_iterator = values.begin() + ((row + 1) * number_of_columns);
            fb_row.resize(number_of_columns);
            std::transform(row_begin_iterator,
                           row_end_iterator,
                           fb_row.begin(),
                           [](const EdgeWeight duration) -> double {
                               if (duration == MAXIMAL_EDGE_DURATION)
                               {
                                   return MAXIMAL_EDGE_DURATION;
                               }
                               // division by 10 because the duration is in deciseconds (10s)
                               return duration / 10.;
                           });
            fb_table.push_back(fbresult::CreateVectorDoubleDirect(builder, &fb_row));
        }
        return builder.CreateVector(fb_table);
    }

    virtual flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::VectorDouble>>>
    MakeDistanceTable(flatbuffers::FlatBufferBuilder& builder,
                      const std::vector<EdgeDistance> &values,
                      std::size_t number_of_rows,
                      std::size_t number_of_columns) const
    {
        std::vector<flatbuffers::Offset<fbresult::VectorDouble>> fb_table;
        for (const auto row : util::irange<std::size_t>(0UL, number_of_rows))
        {
            std::vector<double> fb_row;
            auto row_begin_iterator = values.begin() + (row * number_of_columns);
            auto row_end_iterator = values.begin() + ((row + 1) * number_of_columns);
            fb_row.resize(number_of_columns);
            std::transform(row_begin_iterator,
                           row_end_iterator,
                           fb_row.begin(),
                           [](const EdgeDistance distance) -> double {
                               if (distance == INVALID_EDGE_DISTANCE) {
                                   return INVALID_EDGE_DISTANCE;
                               }
                               // round to single decimal place
                               return std::round(distance * 10) / 10.;
                           });
            fb_table.push_back(fbresult::CreateVectorDoubleDirect(builder, &fb_row));
        }
        return builder.CreateVector(fb_table);
    }

    virtual flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::VectorDouble>>>
    MakeEstimatesTable(flatbuffers::FlatBufferBuilder& builder, const std::vector<TableCellRef> &fallback_speed_cells) const
    {
        std::vector<flatbuffers::Offset<fbresult::VectorDouble>> fb_table;
        fb_table.reserve(fallback_speed_cells.size());
        std::for_each(
                fallback_speed_cells.begin(), fallback_speed_cells.end(), [&](const auto &cell) {
                    std::vector<double> fb_row;
                    fb_row.push_back(cell.row);
                    fb_row.push_back(cell.column);
                    fb_table.push_back(fbresult::CreateVectorDoubleDirect(builder, &fb_row));
                });
        return builder.CreateVector(fb_table);
    }


    virtual util::json::Array MakeWaypoints(const std::vector<PhantomNode> &phantoms) const
    {
        util::json::Array json_waypoints;
        json_waypoints.values.reserve(phantoms.size());
        BOOST_ASSERT(phantoms.size() == parameters.coordinates.size());

        boost::range::transform(
            phantoms,
            std::back_inserter(json_waypoints.values),
            [this](const PhantomNode &phantom) { return BaseAPI::MakeWaypoint(phantom); });
        return json_waypoints;
    }

    virtual util::json::Array MakeWaypoints(const std::vector<PhantomNode> &phantoms,
                                            const std::vector<std::size_t> &indices) const
    {
        util::json::Array json_waypoints;
        json_waypoints.values.reserve(indices.size());
        boost::range::transform(indices,
                                std::back_inserter(json_waypoints.values),
                                [this, phantoms](const std::size_t idx) {
                                    BOOST_ASSERT(idx < phantoms.size());
                                    return BaseAPI::MakeWaypoint(phantoms[idx]);
                                });
        return json_waypoints;
    }

    virtual util::json::Array MakeDurationTable(const std::vector<EdgeWeight> &values,
                                                std::size_t number_of_rows,
                                                std::size_t number_of_columns) const
    {
        util::json::Array json_table;
        for (const auto row : util::irange<std::size_t>(0UL, number_of_rows))
        {
            util::json::Array json_row;
            auto row_begin_iterator = values.begin() + (row * number_of_columns);
            auto row_end_iterator = values.begin() + ((row + 1) * number_of_columns);
            json_row.values.resize(number_of_columns);
            std::transform(row_begin_iterator,
                           row_end_iterator,
                           json_row.values.begin(),
                           [](const EdgeWeight duration) {
                               if (duration == MAXIMAL_EDGE_DURATION)
                               {
                                   return util::json::Value(util::json::Null());
                               }
                               // division by 10 because the duration is in deciseconds (10s)
                               return util::json::Value(util::json::Number(duration / 10.));
                           });
            json_table.values.push_back(std::move(json_row));
        }
        return json_table;
    }

    virtual util::json::Array MakeDistanceTable(const std::vector<EdgeDistance> &values,
                                                std::size_t number_of_rows,
                                                std::size_t number_of_columns) const
    {
        util::json::Array json_table;
        for (const auto row : util::irange<std::size_t>(0UL, number_of_rows))
        {
            util::json::Array json_row;
            auto row_begin_iterator = values.begin() + (row * number_of_columns);
            auto row_end_iterator = values.begin() + ((row + 1) * number_of_columns);
            json_row.values.resize(number_of_columns);
            std::transform(row_begin_iterator,
                           row_end_iterator,
                           json_row.values.begin(),
                           [](const EdgeDistance distance) {
                               if (distance == INVALID_EDGE_DISTANCE)
                               {
                                   return util::json::Value(util::json::Null());
                               }
                               // round to single decimal place
                               return util::json::Value(
                                   util::json::Number(std::round(distance * 10) / 10.));
                           });
            json_table.values.push_back(std::move(json_row));
        }
        return json_table;
    }

    virtual util::json::Array
    MakeEstimatesTable(const std::vector<TableCellRef> &fallback_speed_cells) const
    {
        util::json::Array json_table;
        std::for_each(
            fallback_speed_cells.begin(), fallback_speed_cells.end(), [&](const auto &cell) {
                util::json::Array row;
                row.values.push_back(util::json::Number(cell.row));
                row.values.push_back(util::json::Number(cell.column));
                json_table.values.push_back(std::move(row));
            });
        return json_table;
    }

    const TableParameters &parameters;
};

} // ns api
} // ns engine
} // ns osrm

#endif
