#ifndef BOOST_AFIO_PATH_HPP
#define BOOST_AFIO_PATH_HPP

#include "afio.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#ifdef STD_MTX
#include <mutex>
#else
#include <boost/mutex.hpp>
#endif
#include <boost/smart_ptr/detail/spinlock.hpp>


namespace boost{
	namespace afio{

		class Path
		{
			//typedef std::filesystem::path path;
			typedef boost::afio::directory_entry directory_entry;
			typedef std::function<void(dir_event)> Handler; 
			typedef unsigned long event_no;
			typedef unsigned int event_flag;
			typedef boost::filesystem::path dir_path;
			struct event_flags
			{
				event_flag modified	: 1;		//!< When an entry is modified
				event_flag created	: 1;		//!< When an entry is created
				event_flag deleted	: 1;		//!< When an entry is deleted
				event_flag renamed	: 1;		//!< When an entry is renamed
				event_flag attrib	: 1;		//!< When the attributes of an entry are changed 
				event_flag security	: 1;		//!< When the security of an entry is changed
			};

			struct dir_event
			{
				event_no eventNo;				//!< Non zero event number index
				event_flags flags;				//!< bit-field of director events
				dir_path path; 					//!< Path to event/file

				dir_event() : eventNo(0) { flags.modified = false; flags.created = false; flags.deleted = false; flags.security = false; }
				dir_event(int) : eventNo(0) { flags.modified = true; flags.created = true; flags.deleted = true; flags.security = true; }
				operator event_flags() const throw()
				{
					return this->flags;
				}
				//! Sets the modified bit
				dir_event &setModified(bool v=true) throw()		{ flags.modified=v; return *this; }
				//! Sets the created bit
				dir_event &setCreated(bool v=true) throw()		{ flags.created=v; return *this; }
				//! Sets the deleted bit
				dir_event &setDeleted(bool v=true) throw()		{ flags.deleted=v; return *this; }
				//! Sets the renamed bit
				dir_event &setRenamed(bool v=true) throw()		{ flags.renamed=v; return *this; }
				//! Sets the attrib bit
				dir_event &setAttrib(bool v=true) throw()		{ flags.attrib=v; return *this; }
				//! Sets the security bit
				dir_event &setSecurity(bool v=true) throw()		{ flags.security=v; return *this; }
			};//end dir_event

		private:
			//private data
			boost::mutex mtx;
			boost::detail::spinlock sp_lock;
			std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher;
			//std::weak_ptr<boost::afio::dir_monitor> monitor;
			path name;
			std::unordered_map<directory_entry, directory_entry> dict;
			//std::shared_ptr<std::vector<directory_entry>> ents;
			std::unordered_map<Handler*, Handler> handlers;

			//private member functions
			bool remove_ent(const directory_entry& ent);
			bool add_ent(const directory_entry& ent);
			void schedule();
			bool add_handler(const Handler& h);
			bool remove_handler(const Handler& h);
			void monitor(const boost::system::error_code& ec);
			void compare_entries(directory_entry& entry, std::shared_ptr< async_io_handle> dirh);
			

		public:

			//constructors
			Path(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> _dispatcher, const dir_path& _path): dispatcher(_dispatcher), name(std::filesystem::absolute(_path)) 
			{
				auto dir(dispatcher->dir(boost::afio::async_path_op_req( name)));		
	    		std::pair<std::vector<boost::afio::directory_entry>, bool> list;
	    		async_io_op my_op;
			    bool restart=true;
			    do{				        
			        auto enumerate(dispatcher->enumerate(dir,boost::afio::directory_entry::compatibility_maximum()));
			        restart=false;
			        list=enumeration.first.get();			        
			        my_op = enumerate.second;
			    } while(list.second);

			    std::vector<async_io_op> ops;
			    ops.reserve(list.first.size());
			    std::vector<std::function<void()>> funcs;
			    funcs.reserve(list.first.size());
			    BOOST_FOREACH(auto &i, list.first)
			    {
			    	ops.push_back(my_op);
			    	funcs.push_back(std::bind(add_ent, this, i));
			    }
			    auto make_dict(dispatcher->call(ops, funcs));
			    when_all(make_dict.first.begin(), make_dict.first.end()).wait();
			    

			}

			Path(const Path& o): name(std::filesystem::absolute(o.name), dispatcher(o.dispatcher), dict(o.dict), ents(o.ents), handlers(o.handlers) {}
			Path(Path&& o):  name(std::move(std::filesystem::absolute(o.name))), dispatcher(o.dispatcher)), dict(std::move(o.dict)), entsstd::move(o.ents)), handlers(std::move(o.handlers)) {}


			// public member functions
			std::pair< future< bool >, async_io_op > remove_ent(const async_io_op & req, const directory_entry& ent);
			std::pair< future< bool >, async_io_op > add_ent(const async_io_op & req, const directory_entry& ent);
			std::pair< future< void >, async_io_op > schedule(const async_io_op & req);
			std::pair< future< bool >, async_io_op > add_handler(const async_io_op & req, const Handler& h);
			std::pair< future< bool >, async_io_op > remove_handler(const async_io_op & req, const Handler& h);
		};

	}
}
			

#endif