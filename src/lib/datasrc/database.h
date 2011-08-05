// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef __DATABASE_DATASRC_H
#define __DATABASE_DATASRC_H

#include <datasrc/client.h>

#include <exceptions/exceptions.h>

namespace isc {
namespace datasrc {

/**
 * \brief Abstract connection to database with DNS data
 *
 * This class is defines interface to databases. Each supported database
 * will provide methods for accessing the data stored there in a generic
 * manner. The methods are meant to be low-level, without much or any knowledge
 * about DNS and should be possible to translate directly to queries.
 *
 * On the other hand, how the communication with database is done and in what
 * schema (in case of relational/SQL database) is up to the concrete classes.
 *
 * This class is non-copyable, as copying connections to database makes little
 * sense and will not be needed.
 *
 * \todo Is it true this does not need to be copied? For example the zone
 *     iterator might need it's own copy. But a virtual clone() method might
 *     be better for that than copy constructor.
 *
 * \note The same application may create multiple connections to the same
 *     database. If the database allows having multiple open queries at one
 *     connection, the connection class may share it.
 */
class DatabaseConnection : boost::noncopyable {
public:
    /**
     * \brief Destructor
     *
     * It is empty, but needs a virtual one, since we will use the derived
     * classes in polymorphic way.
     */
    virtual ~ DatabaseConnection() { }
    /**
     * \brief Retrieve a zone identifier
     *
     * This method looks up a zone for the given name in the database. It
     * should match only exact zone name (eg. name is equal to the zone's
     * apex), as the DatabaseClient will loop trough the labels itself and
     * find the most suitable zone.
     *
     * It is not specified if and what implementation of this method may throw,
     * so code should expect anything.
     *
     * \param name The name of the zone's apex to be looked up.
     * \return The first part of the result indicates if a matching zone
     *     was found. In case it was, the second part is internal zone ID.
     *     This one will be passed to methods finding data in the zone.
     *     It is not required to keep them, in which case whatever might
     *     be returned - the ID is only passed back to the connection as
     *     an opaque handle.
     */
    virtual std::pair<bool, int> getZone(const isc::dns::Name& name) const = 0;
    /**
     * \brief This holds the internal context of ZoneIterator for databases
     *
     * While the ZoneIterator implementation from DatabaseClient does all the
     * translation from strings to DNS classes and validation, this class
     * holds the pointer to where the database is at reading the data.
     *
     * It can either hold shared pointer to the connection which created it
     * and have some kind of statement inside (in case single database
     * connection can handle multiple concurrent SQL statements) or it can
     * create a new connection (or, if it is more convenient, the connection
     * itself can inherit both from DatabaseConnection and IteratorContext
     * and just clone itself).
     */
    class IteratorContext : public boost::noncopyable {
    public:
        /**
         * \brief Destructor
         *
         * Virtual destructor, so any descendand class is destroyed correctly.
         */
        virtual ~IteratorContext() { }
        /**
         * \brief Function to provide next resource record
         *
         * This function should provide data about the next resource record
         * from the iterated zone. The data are not converted yet.
         *
         * The order of RRs is not strictly set, but the RRs for single RRset
         * must not be interlieved with any other RRs (eg. RRsets must be
         * "together").
         *
         * \param name The name of the RR will be returned here.
         * \param rtype The string representation of RRType will be returned
         *     through this parameter.
         * \param ttl The time to live output parameter.
         * \param data This is where the string representation of data will be
         *     put.
         * \return If there was RR returned. Once it returns false, the zone
         *     was iterated to its end.
         * \todo Do we consider databases where it is stored in binary blob
         *     format?
         */
        virtual bool getNext(std::string& name, std::string& rtype, int& ttl,
                             std::string& data) = 0;
    };
    typedef boost::shared_ptr<IteratorContext> IteratorContextPtr;
    /**
     * \brief Creates an iterator context for given zone.
     *
     * This should create a new iterator context to be used by
     * DatabaseConnection's ZoneIterator. It can be created based on the name
     * or the ID (returned from getZone()), what is more comfortable for the
     * database implementation. Both are provided (and are guaranteed to match,
     * the DatabaseClient first looks up the zone ID and then calls this).
     *
     * The default implementation throws isc::NotImplemented, to allow
     * "minimal" implementations of the connection not supporting optional
     * functionality.
     *
     * \param name The name of the zone.
     * \param id The ID of the zone, returned from getZone().
     * \return Newly created iterator context. Must not be NULL.
     */
    virtual IteratorContextPtr getIteratorContext(const isc::dns::Name& name,
                                                  int id) const
    {
        /*
         * This is a compromise. We need to document the parameters in doxygen,
         * so they need a name, but then it complains about unused parameter.
         * This is a NOP that "uses" the parameters.
         */
        static_cast<void>(name);
        static_cast<void>(id);

        isc_throw(isc::NotImplemented,
                  "This database datasource can't be iterated");
    }
};

/**
 * \brief Concrete data source client oriented at database backends.
 *
 * This class (together with corresponding versions of ZoneFinder,
 * ZoneIterator, etc.) translates high-level data source queries to
 * low-level calls on DatabaseConnection. It calls multiple queries
 * if necessary and validates data from the database, allowing the
 * DatabaseConnection to be just simple translation to SQL/other
 * queries to database.
 *
 * While it is possible to subclass it for specific database in case
 * of special needs, it is not expected to be needed. This should just
 * work as it is with whatever DatabaseConnection.
 */
class DatabaseClient : public DataSourceClient {
public:
    /**
     * \brief Constructor
     *
     * It initializes the client with a connection.
     *
     * It throws isc::InvalidParameter if connection is NULL. It might throw
     * standard allocation exception as well, but doesn't throw anything else.
     *
     * \note Some objects returned from methods of this class (like ZoneFinder)
     *     hold references to the connection. As the lifetime of the connection
     *     is bound to this object, the returned objects must not be used after
     *     descruction of the DatabaseClient.
     *
     * \todo Should we use shared_ptr instead? On one side, we would get rid of
     *     the restriction and maybe could easy up some shutdown scenarios with
     *     multi-threaded applications, on the other hand it is more expensive
     *     and looks generally unneeded.
     *
     * \param connection The connection to use to get data. As the parameter
     *     suggests, the client takes ownership of the connection and will
     *     delete it when itself deleted.
     */
    DatabaseClient(std::auto_ptr<DatabaseConnection> connection);
    /**
     * \brief Corresponding ZoneFinder implementation
     *
     * The zone finder implementation for database data sources. Similarly
     * to the DatabaseClient, it translates the queries to methods of the
     * connection.
     *
     * Application should not come directly in contact with this class
     * (it should handle it trough generic ZoneFinder pointer), therefore
     * it could be completely hidden in the .cc file. But it is provided
     * to allow testing and for rare cases when a database needs slightly
     * different handling, so it can be subclassed.
     *
     * Methods directly corresponds to the ones in ZoneFinder.
     */
    class Finder : public ZoneFinder {
    public:
        /**
         * \brief Constructor
         *
         * \param connection The connection (shared with DatabaseClient) to
         *     be used for queries (the one asked for ID before).
         * \param zone_id The zone ID which was returned from
         *     DatabaseConnection::getZone and which will be passed to further
         *     calls to the connection.
         */
        Finder(DatabaseConnection& connection, int zone_id);
        virtual isc::dns::Name getOrigin() const;
        virtual isc::dns::RRClass getClass() const;
        virtual FindResult find(const isc::dns::Name& name,
                                const isc::dns::RRType& type,
                                isc::dns::RRsetList* target = NULL,
                                const FindOptions options = FIND_DEFAULT)
            const;
        /**
         * \brief The zone ID
         *
         * This function provides the stored zone ID as passed to the
         * constructor. This is meant for testing purposes and normal
         * applications shouldn't need it.
         */
        int zone_id() const { return (zone_id_); }
        /**
         * \brief The database connection.
         *
         * This function provides the database connection stored inside as
         * passed to the constructor. This is meant for testing purposes and
         * normal applications shouldn't need it.
         */
        const DatabaseConnection& connection() const {
            return (connection_);
        }
    private:
        DatabaseConnection& connection_;
        const int zone_id_;
    };
    /**
     * \brief Find a zone in the database
     *
     * This queries connection's getZone to find the best matching zone.
     * It will propagate whatever exceptions are thrown from that method
     * (which is not restricted in any way).
     *
     * \param name Name of the zone or data contained there.
     * \return Result containing the code and instance of Finder, if anything
     *     is found. Applications should not rely on the specific class being
     *     returned, though.
     */
    virtual FindResult findZone(const isc::dns::Name& name) const;
    /**
     * \brief Get the zone iterator
     *
     * The iterator allows going through the whole zone content. If the
     * underlying DatabaseConnection is implemented correctly, it should
     * be possible to have multiple ZoneIterators at once and query data
     * at the same time.
     *
     * \exception DataSourceError if the zone doesn't exist.
     * \exception isc::NotImplemented if the underlying DatabaseConnection
     *     doesn't implement iteration.
     * \exception Anything else the underlying DatabaseConnection might
     *     want to throw.
     * \param name The origin of the zone to iterate.
     * \return Shared pointer to the iterator (it will never be NULL)
     */
    virtual ZoneIteratorPtr getIterator(const isc::dns::Name& name) const;
private:
    /// \brief Our connection.
    const std::auto_ptr<DatabaseConnection> connection_;
};

}
}

#endif
