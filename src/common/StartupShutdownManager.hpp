/*
 *================================================================================
 *
 *    Copyright (c) 2016 Vortex Co.,Ltd.
 *    Unpublished - All rights reserved
 *
 *================================================================================
 *    File description:
 *    Manager for startup and shutdown routines to be called at program
 *    start and exit
 *
 *================================================================================
 *    Date            Name                    Description of Change
 *    07-Oct-2016     Jiamin Xu               Creation
 *================================================================================
 */
#ifndef ARIES_STARTUPSHUTDOWNMANAGER_HPP
#define ARIES_STARTUPSHUTDOWNMANAGER_HPP

/*
 *================================================================================
 *    Class namespaces
 *================================================================================
 */
namespace ARIES
{
    /*!
     *  @brief Class StartupShutdownManager is a utility for managing
     *  callbacks invoked at program and problem startup and completion.
     *  
     *  There are four steps in the lifecycle of managed objects.
     *  
     *  An object is initialized once at the start of a run and a
     *  corresponding finalization is done once at the end of the run.
     *  These methods can be used to create and destroy static state that
     *  is independent of problem; e.g., when running multiple problems
     *  during a single program (i.e., main) execution.
     *  
     *  The lifecycle also has a startup/shutdown step. This is invoked
     *  at least once per run but may be invoked multiple times if
     *  multiple SAMRAI problems are being run within a single execution of the
     *  main function.  These methods should setup/teardown any state that
     *  is problem dependent.
     *  
     *  In order to address dependencies between classes a Handler provides
     *  a priority (ordering) using the getPriority method. Ordering within
     *  the same priority is undefined.
     *  
     *  The StartupShutdowManger::AbstractHandler defines the interface for
     *  the class that is registered.  StartupShutdowManger::Handler is a
     *  helper class provided to simplify the use of the StartupShutdownManager.
     *  One may simply create a static instance of the Handler class, providing
     *  the constructor with callbacks to invoke and priority to use.
     *  
     *  The StartupShutdownManger is normally managed by the AriesManager
     *  class.
     */
    class StartupShutdownManager
    {
    public:
        /*!
         *  @brief Abstract base class for handler interface.
         *
         *  Defines the four methods to invoke in the lifecycle and four
         *  methods to indicate if the interface should be called for that stage.
         */
        class IHandler
        {
        public:
            IHandler() {}                                                /**< @brief Default constructor. */
            virtual ~IHandler() {}                                       /**< @brief Virtual destructor since class has virtual methods. */
            
            virtual void Initialize() = 0;                               /**< @brief Method that is invoked on Initialize. */
            virtual void Startup() = 0;                                  /**< @brief Method that is invoked on Startup. */
            virtual void Shutdown() = 0;                                 /**< @brief Method that is invoked on Shutdown. */
            virtual void Finalize() = 0;                                 /**< @brief Method that is invoked on Finalize. */
            
            virtual unsigned char GetPriority() = 0;                     /**< @brief Get the Priority of this handler. */
 
            virtual bool HasInitialize() = 0;                            /**< @brief Query if handler has Initialize callback function. */
            virtual bool HasStartup() = 0;                               /**< @brief Query if handler has Startup callback function. */
            virtual bool HasShutdown() = 0;                              /**< @brief Query if handler has Shutdown callback function. */
            virtual bool HasFinalize() = 0;                              /**< @brief Query if handler has Finalize callback function. */

        private:
            IHandler(const IHandler& other);                             /**< @brief Unimplemented assignment operator. */
            IHandler& operator = (const IHandler& rhs);                  /**< @brief Unimplemented assignment operator. */
        };

        /*!
         *  @brief Standard implementation of a Startup/Shutdown handler.
         *  
         *  This class is provided to simplify construction of a
         *  handler class for standard ARIES startup/shutdown uses.  This
         *  handler registers itself with the StartupShutdownManager on
         *  construction. On construction the methods to invoke for each
         *  step of the lifecycle are supplied. The method may be 0
         *  indicating that the managed class does not require anything to
         *  be executed at that step.
         *  
         *  Example usage shows how the static initialization of the
         *  d_startupShutdownHandler is used to provide a simple
         *  method of registering a class with the manager that requires
         *  startup and shutdown but not initialization or finalization.
         *  
         *  \code
         *  classs StartupShutdownExample
         *  {
         *  public:
         *      static void StartupCallback()
         *      {
         *         // stuff to do for class initialization.
         *      }
         *  
         *      static void shutdownCallback()
         *      {
         *         // stuff to do for class destruction.
         *      }
         *  private:
         *      static StartupShutdownManger::Handler<StartupShutdownExample> d_startupShutdownHandler;
         *  }
         *  
         *  static unsigned short handlerPriority = 150;
         *  StartupShutdownManger::Handler<StartupShutdownExample> d_startupShutdownHandler(0,
         *                                                                                  StartupShutdownExample::startupCallback,
         *                                                                                  StartupShutdownExample::shutdownCallback,
         *                                                                                  0,
         *                                                                                  handlerPriority);
         *  \endcode
         *  
         *  Note that this mechanism does NOT work for templated classes as
         *  static variables in template classes are only initialized if
         *  used. For template classes a slightly different mechanism using
         *  a static in a method is used. Similar to the Meyer singleton
         *  implementation. This method has the disadvantage of having to check
         *  on each object construction if the static has been created (it is
         *  done under the hood by the compiler but still exists). At this
         *  time we are not aware of a method to invoke a block of code only
         *  once for a template class.
         *
         */
        class Handler: public IHandler
        {
        public:
            /*!
             *  @brief Construct a handler with the callbacks provided and
             *  specified priority.
             *
             *  The callback function pointers should be NULL for callbacks not
             *  required for a particular class.
             *
             *  @param[in] initialize   Initialization callback function.
             *  @param[in] startup      Startup callback function.
             *  @param[in] shutdown     Shutdown callback function.
             *  @param[in] finalize     Finalization callback function.
             *  @param[in] priority
             *
             */
            Handler(void (*initialize)(),
                    void (*startup)(),
                    void (*shutdown)(),
                    void (*finalize)(),
                    unsigned char priority);

            ~Handler();                                                     /**< @brief Destructor. */

            void Initialize();                                              /**< @brief Method that is invoked on Initialize. */
            void Startup();                                                 /**< @brief Method that is invoked on Startup. */
            void Shutdown();                                                /**< @brief Method that is invoked on Shutdown. */
            void Finalize();                                                /**< @brief Method that is invoked on Finalize. */

            unsigned char GetPriority();

            bool HasInitialize();                                           /**< @brief Query if handler has Initialize callback function. */
            bool HasStartup();                                              /**< @brief Query if handler has Startup callback function. */
            bool HasShutdown();                                             /**< @brief Query if handler has Shutdown callback function. */
            bool HasFinalize();                                             /**< @brief Query if handler has Finalize callback function. */

        private:
            Handler();                                                      /**< @brief Unimplemented default constructor. */
            Handler(const Handler& other);                                  /**< @brief Unimplemented copy constructor. */
            Handler& operator = (const Handler& rhs);                       /**< @brief Unimplemented assignment operator. */

            void (*d_initialize)();                                         /**< @brief Initialize function. */
            void (*d_startup)();                                            /**< @brief Startup function. */
            void (*d_shutdown)();                                           /**< @brief Shutdown function. */
            void (*d_finalize)();                                           /**< @brief Finalize function. */

            unsigned char d_priority;                                       /**< @brief Priority of the handler. */
        };

        /*!
         *  @brief Register a handler with the StartupShutdownManager.
         *  
         *  The IHandler interface defines callback methods that will be
         *  invoked on ARIES initialize, startup and shutdown, and
         *  finalize.
         *  
         *  The IHandler also defines a priority (via getPriority)
         *  used to specify the order for startup and shutdown.  Lower
         *  numbers are started first, higher last (0 first, 254 last).
         *  Order is inverted on shutdown (254 first, 0 last).
         *  
         *  The priority is required since handlers may have dependencies.
         *  
         *  Users are reserved priorities 127 to 254. Unless there is a
         *  known dependency on a SAMRAI shutdown handler, users should use
         *  these priorities.
         *  
         *  Note: Currently it is allowed to register a handler in a phase
         *  if the handler does not have a callback for that phase. In
         *  other words if during the startup callback of a class A it
         *  causes another class B to register a handler it will work only
         *  if the handler for B does not have a startup method (hasStartup
         *  returns false). This restriction is in place to prevent one
         *  from registering a handler during startup that needs to be
         *  started.  This should be avoided but for legacy reasons is being
         *  done in ARIES.
         *  
         *  @param handler
         */
        static void RegisterHandler(IHandler* handler);

        /*!
         *  @brief Invoke the registered initialization handlers.
         *
         *  This should only be called once per program execution.
         */
        static void Initialize();

        /*!
         *  @brief Invoke the registered startup handlers.
         *
         *  This routine must be called at SAMRAI problem startup.  It may be called
         *  more than once per run if running multiple SAMRAI problems within the
         *  same execution of the main function.
         */
        static void Startup();

        /*!
         *  @brief Invoke the registered shutdown handlers.
         *
         *  This routine must be called at SAMRAI problem shutdown.  It may be called
         *  more than once per run if running multiple SAMRAI problems within the
         *  same execution of the main function.
         */
        static void Shutdown();

        /*!
         *  @brief Invoke the registered finalize handlers.
         *
         *  This should only be called once per program execution.
         */
        static void Finalize();

        //@{
        //!  @name Priorities for standard ARIES classes
        static const unsigned char priorityArenaManager = 10;
        static const unsigned char priorityReferenceCounter = 20;
        static const unsigned char priorityLogger = 25;
        static const unsigned char priorityListElements = 30;
        static const unsigned char priorityList = 30;
        static const unsigned char priorityInputManager = 40;
        static const unsigned char priorityRestartManager = 50;
        static const unsigned char priorityVariableDatabase = 60;
        static const unsigned char priorityStatistician = 70;
        static const unsigned char priorityBoundaryLookupTable = 80;
        static const unsigned char priorityHierarchyDataOpsManager = 90;
        static const unsigned char priorityTimerManger = 95;
        static const unsigned char priorityTimers = 98;
        static const unsigned char priorityVariables = 100;
        //@}

    private:
        StartupShutdownManager();                                                    /**< @brief Unimplemented default constructor. */
        StartupShutdownManager(const StartupShutdownManager& other);                 /**< @brief Unimplemented copy constructor. */
        StartupShutdownManager& operator = (const StartupShutdownManager& rhs);      /**< @brief Unimplemented assignment operator. */

        static void SetupSingleton();                                                /**< @brief Sets up the StartupShutdownManager singleton. */

        /*!
         *  @brief Used to maintain a list of registered handlers.
         */
        class ListElement
        {
        public:
            ListElement();                                                           /**< @brief Constructor. */
            ListElement(const ListElement& other);                                   /**< @brief Unimplemented copy constructor. */
            ListElement& operator = (const ListElement& rhs);                        /**< @brief Unimplemented assignment operator. */
            ~ListElement();                                                          /**< @brief Destructor. */

            StartupShutdownManager::IHandler* handler;                               /**< @brief A registered handler being stored in a list.*/
            ListElement* next;                                                       /**< @brief Next element of the list. */
        };

        static const short d_numberOfPriorities = 255;  /**< @brief Maximum value of handler priorities. */

        static ListElement* d_managerList[d_numberOfPriorities];     /**< @brief Array of singly linked lists holding registered handlers. */
        static ListElement* d_managerListLast[d_numberOfPriorities]; /**< @brief Array of pointers to ListElement, used for constructing and modifying s_manager_list. */

        static int d_numManagerItems[d_numberOfPriorities];      /**< @brief Number of items at each priority value. */
        
        static bool d_singletonInitialized;       /**< @brief Flag telling if the singleton been initialized. */
        
        static bool d_inInitialize;               /**< @brief Flags telling if the manager is currently in one of the loops invoking callbacks. */
        static bool d_inStartup;                  /**< @brief Flags telling if the manager is currently in one of the loops invoking callbacks. */
        static bool d_inShutdown;                 /**< @brief Flags telling if the manager is currently in one of the loops invoking callbacks. */
        static bool d_inFinalize;                 /**< @brief Flags telling if the manager is currently in one of the loops invoking callbacks. */
                                                   
        static bool d_initialized;                /**< @brief Flags telling which methods have been invoked. Generally used only for error checking. */
        static bool d_startuped;                  /**< @brief Flags telling which methods have been invoked. Generally used only for error checking. */
        static bool d_shutdowned;                 /**< @brief Flags telling which methods have been invoked. Generally used only for error checking. */ 
        static bool d_finalized;                  /**< @brief Flags telling which methods have been invoked. Generally used only for error checking. */
        
    }; // end class StartupShutdownManager
} // end namespace ARIES

#endif

