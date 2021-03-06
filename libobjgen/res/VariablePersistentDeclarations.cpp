virtual std::list<libcomp::DatabaseBind*> GetMemberBindValues(bool retrieveAll = false, bool clearChanges = true);
virtual bool LoadDatabaseValues(libcomp::DatabaseQuery& query);
virtual std::shared_ptr<libobjgen::MetaObject> GetObjectMetadata();
static std::shared_ptr<libobjgen::MetaObject> GetMetadata();
