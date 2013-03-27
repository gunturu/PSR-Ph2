//*=============================================================================
        //        Copyright (c) 2003-2005 UGS Corporation
          //         Unpublished - All Rights Reserved
/*===============================================================================
File description:

    File   : psr.cxx
    Module : main
 */

#include <itk/mem.h>
#include <tcinit/tcinit.h>
#include <bom/bom.h>
#include <ecm/ecm.h>
#include <epm/epm.h>
#include <pom/pom/pom.h>
#include <tccore/tctype.h>
#include <sa/tcvolume.h>
#include <tc/emh.h>
#include <textsrv/textserver.h>
#include <pom/enq/enq.h>
#include <tccore/item.h>
#include <tccore/grm.h>
#include <tccore/workspaceobject.h>
#include <fclasses/tc_stdio.h>
#include <ae/vm_errors.h>
#include <tccore/item_errors.h>
#include <tc/tc_macros.h>
#include <fclasses/tc_date.h>
#include <fclasses/tc_string.h>
#include <list>
#include <tccore/aom.h>
#include <cfm.h>
#include <tccore/aom_prop.h>
#include <ict/ict_userservice.h>
#include <ae/datasettype.h>
#include <ae/dataset.h>
#include <stdio.h>
#include <stdlib.h>
#include <ps/ps.h>
#include <bom/bom.h>
#include <cfm/cfm.h>
#include <tie/tie.h>
#include <epm/signoff.h>
#include <epm/cr.h>
#include <string>
#include <vector>
#include <epm/epm_errors.h>
#include <epm/epm_task_template_itk.h>
using namespace std;





/************************************************************************************/
const int MAX_TDS = 5;
static void display_usage();
int debug=FALSE;
char *my_name="psr";
char *system_cmd=NULL;
static int prog_debug=0;
static char* filename;
static void dump_itk_errors( int stat, const char * prog_name, int lineNumber, const char * fileName );

//static int get_spec_info( tag_t revtag, spec_info * s1 );
FILE *LOGFILE = NULL;
/*
 * The macro "ITK" is a wrapper for all ITK calls: It submitts the call `(x)'
 * only if the variable `stat' (must be defined outside!) is equal to ITK_ok.
 * This way we prevent to continue to submit ITK calls if an error occured.
 * For convenience this macro prints out the IMAN error string (don't forget
 * to call `ITK_initialize_text_services()' prior to `ITK_auto_login()'), the
 * source file name (__FILE__) and the source code line number (__LINE__).
 *
 * Note: __LINE__ and  __FILE__ are replaced with the actual values during the
 * cpp (c pre processor) run.
 */
#define ITK(x)                                                             \
{                                                                          \
    if ( stat == ITK_ok )                                                  \
    {                                                                      \
        if ( (stat = (x)) != ITK_ok )                                      \
        {                                                                  \
            dump_itk_errors ( stat, my_name, __LINE__, __FILE__ );         \
        }                                                                  \
    }                                                                      \
}

/////////////////////////////////////////////////////////////////////////////
// Called by the ITK macro to log errors.
/////////////////////////////////////////////////////////////////////////////
static void dump_itk_errors( int stat, const char * prog_name, int lineNumber, const char * fileName )
{
    int          n_ifails=0;
    const int   *severities=NULL;
    const int   *ifails=NULL;
    const char **texts=NULL;
    char        *errstring=NULL;

    EMH_ask_errors( &n_ifails, &severities, &ifails, &texts );
    if ( n_ifails && texts != NULL )
    {
        if ( ifails[n_ifails-1] == stat )
        {
            fprintf( stderr, "%s: Error %d: %s\n", prog_name, stat, texts[n_ifails-1] );
        }
        else
        {
            EMH_ask_error_text (stat, &errstring );
            fprintf( stderr, "%s: Error %d: %s\n", prog_name, stat, errstring );
            MEM_free( errstring );
        }
    }
    else
    {
        EMH_ask_error_text (stat, &errstring );
        fprintf( stderr, "%s: Error %d: %s\n", prog_name, stat, errstring );
        MEM_free( errstring );
    }
    fprintf( stderr, "%s: Error: Line %d in %s\n", prog_name, lineNumber, fileName );
}


/************************************************************************************/
/*
 *   This function will display the usage of psr
 */

static void display_usage()
{
    printf("\nUsage is: psr [-h] -u=<username> -p=<password> -g=<groupname> -l=<log_file> -id=<project id>\n");
    printf("       -h      displays detailed help information\n\n");
    printf("       -d      display detailed debug information\n\n");
    return;
}


typedef struct
{
    char *name;
    char *decision;
}reviewer_info;

class spec
{
private:
    string m_status;
    bool m_is_late;
    bool m_found;
    date_t due_date;
    date_t signoff_date;
    int  num_reviewers;
    vector<reviewer_info> reviewers1;
    string m_link;
    static const char *process_stageList;
    static const char *release_statusList;
public:
    reviewer_info *reviewers;

    spec(tag_t revtag )
    {
        get_spec_info( revtag );
    }
    spec();   // Default Class Constructor
    ~spec();  // Class Destructor

    int get_spec_info( tag_t revtag );

    std::string getStatus() const
    {
        return m_status;
    }
    std::string getLink() const
    {
        return m_link;
    }
    bool getIsLate() const
    {
        return m_is_late;
    }
    bool isFound() const
    {
        return m_found;
    }
    date_t getDueDate() const
    {
        return due_date;
    }
    date_t getSignoffDate() const
    {
        return signoff_date;
    }
    int getNumReviewrs() const
    {
        return num_reviewers;
    }
    std::vector<reviewer_info> getReviewers()
    {
        return reviewers1;
    }
};

const char* spec::process_stageList = "process_stage_list";
const char* spec::release_statusList = "release_status_list";
spec :: spec()
{
    m_is_late = false;
    m_found = false;
    date_t due_date = NULLDATE;
    date_t signoff_date = NULLDATE;
    int  num_reviewers = 0;
    vector<reviewer_info> reviewers1;
}
typedef struct
{
    spec srs_info;
    spec fs_info;
    spec scs_info;
    spec sdd_info;
    spec ptp_info;
    spec pdr_info;
    spec boe_info;
    spec tds_info[MAX_TDS];
    int num_tds_docs;
} proj_info;

class PSR_Project
{
private:
    string m_id;
    string m_name;
    string m_desc;
    string m_owner;
    string m_portfolioValue;
    string m_POR;
    string m_devManager;
    string m_devManagerEmail;
    string m_domManager;
    string m_domManagerEmail;
    double m_manMonth;
    string m_QALead;
    string m_QALeadEmail;
    string m_tDocLead;
    string m_tDocLeadEmail;
    string m_projectList;
    string m_productManager;
    string m_productManagerEmail;
    string m_primArch;
    string m_reviewArch;
    int m_approved_usecases;
    int m_unapproved_usecases;
    double m_percent_usecases_approved;
    string m_projectCategory;
    string m_releaseName;
    string m_reviewDate;
    string m_projectStatus;
    string m_projectType;
    string m_url;

    int dep1_count; //Projects that this project depends on
    vector <std::string>dep1_proj;

    int dep2_count;//Projects that depend on this project
    vector <std::string> dep2_proj;

    static const char *projectId;
    static const char *projectName;
    static const char *projecDes;
    static const char *owingUser;
    static const char *orgProject;
    static const char *devManager;
    static const char *DomainManager;
    static const char *qaLead;
    static const char *tDocLead;
    static const char *manMon;
    static const char *prodMngr;
    static const char *projectList;
    static const char *primArch;
    static const char *reviewingArch;
    static const char *projectStatus;
    static const char *projectType;
    static const char *projectCategory;
    static const char *doDate;
    static const char *Relation;
    static const char *maturity;
    static const char *busUsecase;
    static const char *asRequiredRevision;
    static const char *ReleaseRevision;
    static const char *ReleaseName;
    static const char *portfolioElement;
    static const char *docSDD;
    static const char *docSRS;
    static const char *docFS;
    static const char *docSCS;
    static const char *docPDR;
    static const char *docTDS;
    static const char *docPTP;
    static const char *docBOE;
    static const char *devProject;

public:
    proj_info *specs_info;
    int get_pbp_info( char* item_id );
    int get_pbb_info( tag_t child_rev_tag );
    int get_proj_info( tag_t child_rev_tag, char item_type[ITEM_type_size_c+1], int tds_ctr, bool *is_incremented );
    PSR_Project();   // Default Class Constructor
    ~PSR_Project();  // Class Destructor

    // to get item id of development project
    std::string getId() const
    {
        return m_id;
    }
    // to get name of development project
    std::string getName() const
    {
        return m_name;
    }
    // to get desc of development project
    std::string getDesc() const
    {
        return m_desc;
    }
    // to get owner of development project
    std::string getOwner() const
    {
        return m_owner;
    }
    // to get plan of record of development project
    std::string getPOR() const
    {
        return m_POR;
    }
    // to get portfolio element of development project
    std::string getPRE() const
    {
        return m_portfolioValue;
    }
    // to get dev manager name of development project
    std::string getDevManager() const
    {
        return m_devManager;
    }
    // to get dev manager email of development project
    std::string getDevManagerEmail() const
    {
        return m_devManagerEmail;
    }
    // to get domain manager name of development project
    std::string getDomManager() const
    {
        return m_domManager;
    }
    // to get domain manager email of development project
    std::string getDomManagerEmail() const
    {
        return m_domManagerEmail;
    }
    // to get QA lead name of development project
    std::string getQALead() const
    {
        return m_QALead;
    }
    // to get QA lead email of development project
    std::string getQALeadEmail() const
    {
        return m_QALeadEmail;
    }
    // to get tDocLead name of development project
    std::string gettDocLead() const
    {
        return m_tDocLead;
    }
    // to get tDoc Lead email of development project
    std::string gettDocLeadEmail() const
    {
        return m_tDocLeadEmail;
    }
    // to get list of development project
    std::string getProjectList() const
    {
        return m_projectList;
    }
    // to get product manager of development project
    std::string getprodManager() const
    {
        return m_productManager;
    }
    // to get product manager email of development project
    std::string getprodManagerEmail() const
    {
        return m_productManagerEmail;
    }
    // to get architech of development project
    std::string getprimArch() const
    {
        return m_primArch;
    }
    // to get review architec names of development project
    std::string getreviewArch() const
    {
        return m_reviewArch;
    }
    // to get status of development project
    std::string getProjectStatus() const
    {
        return m_projectStatus;
    }
    // to get Category of development project
    std::string getProjectCategory() const
    {
        return m_projectCategory;
    }
    // to get type of development project
    std::string getProjectType() const
    {
        return m_projectType;
    }
    // to get review date of development project
    std::string getreviewDate() const
    {
        return m_reviewDate;
    }
    // to get release name of development project
    std::string getreleaseName() const
    {
        return m_releaseName;
    }
    // to get man month of development project
    double getManMonth() const
    {
        return m_manMonth;
    }
    // to get approved use cases of development project
    int getapproved_usecases() const
    {
        return m_approved_usecases;
    }
    // to get unapproved use cases of development project
    int getunapproved_usecases() const
    {
        return m_unapproved_usecases;
    }
    // to get percentage approved use cases of development project
    double getpercentUsecasesApproved() const
    {
        return m_percent_usecases_approved;
    }
    // to get count of dependent projects to this development project
    int getDep1_count() const
    {
        return dep1_count;
    }
    // to get count of dependent projects to this development project
    int getDep2_count() const
    {
        return dep2_count;
    }
    // to get vector of dependent projects to this development project
    const vector<string> getDep2_proj() const
    {
        return dep2_proj;
    }
    // to get vector of dependent projects to this development project
    const vector<string>  getDep1_proj() const
    {
        return dep1_proj;
    }
    //url of development project
    std::string getUrl() const
    {
        return m_url;
    }
    // to initialize the item id of development project
    PSR_Project(char* item_id)
    {
        get_pbp_info(item_id);
    }
};
//In future if any property names will change we can change these values itself
const char* PSR_Project::projectId = "current_id";
const char* PSR_Project::projectName = "current_name";
const char* PSR_Project::projecDes = "current_desc";
const char* PSR_Project::owingUser = "owning_user";
const char* PSR_Project::orgProject = "SW1_OrigProject";
const char* PSR_Project::devManager = "SW1_Dev_Manager";
const char* PSR_Project::DomainManager = "SW1_Domain_Manager";
const char* PSR_Project::qaLead = "SW1_Test_Engineer";
const char* PSR_Project::tDocLead = "SW1_Doc_Writer";
const char* PSR_Project::manMon = "SW1_Dev_Estimate_To_Finish";
const char* PSR_Project::prodMngr = "SW1_Product_Manager";
const char* PSR_Project::projectList = "project_ids";
const char* PSR_Project::primArch = "SW1_Architect";
const char* PSR_Project::reviewingArch = "SW1_Architect_Other";
const char* PSR_Project::projectStatus = "SW1_Project_Status";
const char* PSR_Project::projectType = "SW1_Project_Type";
const char* PSR_Project::projectCategory = "SW1_GM_CMS_Allocation";
const char* PSR_Project::doDate = "SW1_Design_Review_Date";
const char* PSR_Project::Relation = "FND_TraceLink";
const char* PSR_Project::busUsecase = "SW1_BusUseCaseRevision";
const char* PSR_Project::maturity = "SW1_Maturity";
const char* PSR_Project::asRequiredRevision = "SW1_AsRequiredRevision";
const char* PSR_Project::ReleaseRevision = "SW1_ReleaseRevision";
const char* PSR_Project::ReleaseName = "SW1_Internal_Identifier";
const char* PSR_Project::portfolioElement = "SW1_Portfolio_Element";
const char* PSR_Project::docSDD = "SW1_DocSDD";
const char* PSR_Project::docSRS = "SW1_DocSRS";
const char* PSR_Project::docFS =  "SW1_DocFS";
const char* PSR_Project::docBOE = "SW1_DocBOE";
const char* PSR_Project::docPTP = "SW1_DocPTP";
const char* PSR_Project::docPDR = "SW1_DocPDR";
const char* PSR_Project::docTDS = "SW1_DocTDS";
const char* PSR_Project::docSCS = "SW1_DocSCS";
const char* PSR_Project::devProject = "SW1_DevProjectRevision";
//constructor of PSR_Project
PSR_Project::PSR_Project()
{
    m_manMonth                    = 0.0;
    m_approved_usecases           = 0;
    m_unapproved_usecases         = 0;
    m_percent_usecases_approved   = 0.0;
}
/*
 * This mehod will get all details of MyPLM properties(devManager name and email,domainManager name and Email.....etc)
 * attached to development project item revision.
 */
int PSR_Project::get_pbp_info( char* item_id )
{

    int ifail = ITK_ok;
    tag_t item_tag = NULLTAG;
    tag_t rev_tag = NULLTAG;
    tag_t relation_tag = NULLTAG;
    int count = 0;
    tag_t *secondary_objects = NULL;
    tag_t prop_tag = NULLTAG;
    char *current_id = NULL;
    char *current_name = NULL;
    char *current_desc = NULL;
    char *dev_mangr = NULL;
    char *dom_mgr = NULL;
    char *test_engr = NULL;
    char *doc_wrtr  = NULL;
    char *dev_manager = NULL;
    char *dom_manager = NULL;
    char *qa_lead = NULL;
    char *tdoc_lead = NULL;
    double man_month = 0.0;
    char *product_manager = NULL;
    char *project_list = NULL;
    char *prim_Arch = NULL;
    char **rev_Arch = NULL;
    char *project_type = NULL;
    char *project_status = NULL;
    char *project_category = NULL;
    char *date_string = NULL;
    char *Maturity = NULL;
    date_t do_date = NULLDATE;
    list <tag_t> busUseCaseList;
    list <tag_t>::iterator it;
    logical flag = false;
    tag_t owning_user_tag = NULLTAG;
    char* user_name = NULL;
    int review_num = 0;
    string review_arch;
    int rev_count = 0;


    ifail = ITEM_find_item( item_id, &item_tag );
    if(prog_debug)
    {
        if(item_tag == NULLTAG)
        {
            printf("\n Item tag = %d\n", ifail);
            fprintf(stderr,"\nItem tag ifail and line number = %d %d\n", ifail,__LINE__);
        }
    }
    if ( item_tag != NULLTAG)
    {

        ifail = AOM_ask_value_string(item_tag,projectId,&current_id);
        if(prog_debug)
        {
            if(current_id == NULL)
            {
                printf("\n Item ID ifail is %d \n",ifail);
                fprintf(stderr,"\nItem ID ifail and line number = %d %d\n", ifail,__LINE__);
            }
        }
        if ( current_id != NULL )
        {
            m_id = MEM_string_copy(current_id);
            MEM_free(current_id);
        }
        ifail = AOM_ask_value_string(item_tag,projectName,&current_name);
        if(prog_debug)
        {
            if(current_name == NULL)
            {
                printf("\n Item name ifail is %d \n",ifail);
                fprintf(stderr,"\ncurrent id ifail and line number= %d %d\n", ifail,__LINE__);
            }
        }
        if ( current_name != NULL )
        {
            m_name=MEM_string_copy(current_name);
            MEM_free(current_name);
        }
        ifail = AOM_ask_value_string(item_tag,projecDes,&current_desc);
        if(prog_debug)
        {
            if(current_desc == NULL)
            {
                printf("\n Item desc ifail is %d \n",ifail);
                fprintf(stderr,"\nItem desc ifail and line number = %d %d\n", ifail,__LINE__);
            }
        }
        if ( current_desc != NULL )
        {
            m_desc = MEM_string_copy(current_desc);
            MEM_free(current_desc);
        }
        ifail = AOM_ask_value_tag(item_tag,owingUser,&owning_user_tag);
        if(prog_debug)
        {
            if(owning_user_tag == NULLTAG)
            {
                printf("\n owining user ifail is %d \n",ifail);
                fprintf(stderr,"\nowining user ifail and line number = %d %d\n", ifail,__LINE__);
            }
        }
        if(owning_user_tag != NULLTAG)
        {
            ifail = POM_ask_user_name ( owning_user_tag, &user_name );
        }
        if(prog_debug)
        {
            if(user_name == NULL)
            {
                printf("\n owining user ifail is %d \n",ifail);
                fprintf(stderr,"\nowining user ifail and line number = %d %d\n", ifail,__LINE__);
            }
        }
        if ( user_name != NULL )
        {
            m_owner =  MEM_string_copy(user_name);
            MEM_free(user_name);
        }
        ifail = ITEM_ask_latest_rev( item_tag, &rev_tag );
        if(prog_debug)
        {
            if(rev_tag == NULLTAG)
            {
                printf("\nitem revision ifail is %d %d\n", ifail,__LINE__);
                fprintf(stderr,"\nitem revision ifail and line number is %d %d\n", ifail,__LINE__);
            }
        }
        if ( rev_tag != NULLTAG )
        {
            //ifail = GRM_find_relation_type 	( orgProject, &relation_tag );
            //ifail = GRM_list_primary_objects_only (rev_tag,relation_tag,&num,&secondary);
            //ifail = ITEM_ask_rev_type(secondary[0], rev_type);
            //if(strcmp(rev_type,"SW1_DevProjectRevision")==0)
            // data migrated from phase 1 to phase2 add the above commented code and pass secondary[0] instead of rev_tag
            // remaining all will be same
            int x=0;
            //int y=0;
            string dev_Manager;
            string dom_Manager;
            string qa_Lead;
            string tdoc_Lead;
            string product_Manager;
            string prim_arch;
            string dev_ManagerEmail;
            string dom_ManagerEmail;
            string qa_LeadEmail;
            string tdoc_LeadEmail;
            string product_ManagerEmail;
            ifail = AOM_ask_value_string(rev_tag,devManager,&dev_manager);
            if(prog_debug)
            {
                if(dev_manager == NULL)
                {
                    printf("\ndev manager name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\ndev manager name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(dev_manager != NULL)
            {
                dev_Manager = MEM_string_copy(dev_manager);
                x = dev_Manager.find("(");
                //y = dev_Manager.find(")");
                dev_ManagerEmail = dev_Manager.substr(x+1,x+1);
                if(!dev_ManagerEmail.empty())
                {
                    m_devManagerEmail = dev_ManagerEmail.append("@ugs.com");
                }
                m_devManager = dev_Manager.substr(0,x-1);
                MEM_free(dev_manager);
            }
            ifail = AOM_ask_value_string(rev_tag,DomainManager,&dom_manager);
            if(prog_debug)
            {
                if(dom_manager == NULL)
                {
                    printf("\ndomain manager name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\ndomain manager name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(dom_manager != NULL)
            {
                dom_Manager = MEM_string_copy(dom_manager);
                x = dom_Manager.find("(");
                //y = qa_Lead.find(")");
                dom_ManagerEmail = dom_Manager.substr(x+1,x+1);
                if( !dom_ManagerEmail.empty() )
                {
                    m_domManagerEmail = dom_ManagerEmail.append("@ugs.com");
                }
                m_domManager = dom_Manager.substr(0,x-1);
                MEM_free(dom_manager);
            }
            ifail = AOM_ask_value_string(rev_tag,qaLead,&qa_lead);
            if(prog_debug)
            {
                if(qa_lead == NULL)
                {
                    printf("\nQA lead name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nQA lead name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(qa_lead != NULL)
            {
                qa_Lead = MEM_string_copy(qa_lead);
                x = qa_Lead.find("(");
                //y = qa_Lead.find(")");
                qa_LeadEmail = qa_Lead.substr(x+1,x+1);
                if(!qa_LeadEmail.empty())
                {
                    m_QALeadEmail = qa_LeadEmail.append("@ugs.com");
                }
                m_QALead = qa_Lead.substr(0,x-1);
                MEM_free(qa_lead);
            }
            ifail = AOM_ask_value_string(rev_tag,tDocLead,&tdoc_lead);
            if(prog_debug)
            {
                if(tdoc_lead == NULL)
                {
                    printf("\ntDoc lead name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\ntDoc lead name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(tdoc_lead != NULL)
            {
                tdoc_Lead = MEM_string_copy(tdoc_lead);
                x = tdoc_Lead.find("(");
                //y = tdoc_Lead.find(")");
                tdoc_LeadEmail = tdoc_Lead.substr(x+1,x+1);
                if(!tdoc_LeadEmail.empty())
                {
                    m_tDocLeadEmail = tdoc_LeadEmail.append("@ugs.com");
                }
                m_tDocLead = tdoc_Lead.substr(0,x-1);
                MEM_free(tdoc_lead);
            }
            ifail = AOM_ask_value_double(rev_tag,manMon,&man_month);
            if(prog_debug)
            {
                if(man_month == 0)
                {
                    printf("\nman_months ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nman_months ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(man_month != 0)
            {
                m_manMonth = man_month;
            }
            ifail = AOM_ask_value_string(rev_tag,prodMngr,&product_manager);
            if(prog_debug)
            {
                if(product_manager == NULL)
                {
                    printf("\nproduction manager name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nproduction manager name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(product_manager != NULL)
            {
                product_Manager = MEM_string_copy(product_manager);
                x = product_Manager.find("(");
                //y = product_Manager.find(")");
                product_ManagerEmail = product_Manager.substr(x+1,x+1);
                if(!product_ManagerEmail.empty())
                {
                    m_productManagerEmail = product_ManagerEmail.append("@ugs.com");
                }
                m_productManager = product_Manager.substr(0,x-1);
                MEM_free(product_manager);
            }
            ifail = AOM_ask_value_string(rev_tag,projectList,&project_list);
            if(prog_debug)
            {
                if(project_list == NULL)
                {
                    printf("\nproject list ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nproject list ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(project_list != NULL)
            {
                m_projectList = MEM_string_copy(project_list);
                MEM_free(project_list);
            }
            ifail = AOM_ask_value_string(rev_tag,primArch,&prim_Arch);
            if(prog_debug)
            {
                if(prim_Arch == NULL)
                {
                    printf("\nArchitec name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nArchitec name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(prim_Arch != NULL)
            {
                prim_arch = MEM_string_copy(prim_Arch);
                x = prim_arch.find("(");
                //y = prim_arch.find(")");
                m_primArch = prim_arch.substr(0,x-1);
                MEM_free(prim_Arch);
            }
            ifail = AOM_ask_value_strings(rev_tag,reviewingArch,&review_num,&rev_Arch);
            if(prog_debug)
            {
                if(rev_Arch == NULL)
                {
                    printf("\nReview Architec name ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nReview Architec name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(review_num > 0 && rev_Arch != NULL)
            {
                for(int inx =0; inx<review_num; inx++)
                {
                    rev_count++;
                    review_arch.append(rev_Arch[inx]);
                    if(rev_count != review_num-1)
                        review_arch.append(";");
                }

                MEM_free(rev_Arch);
                m_reviewArch = review_arch;
            }
            ifail = AOM_ask_value_string(rev_tag,projectStatus,&project_status);
            if(prog_debug)
            {
                if(project_status == NULL)
                {
                    printf("\nproject_status ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nproject_status ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(project_status != NULL)
            {
                m_projectStatus = MEM_string_copy(project_status);
                MEM_free(project_status);
            }
            ifail = AOM_ask_value_string(rev_tag,projectType,&project_type);
            if(prog_debug)
            {
                if(project_type == NULL)
                {
                    printf("\nproject type ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nproject type ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(project_type != NULL)
            {
                m_projectType = MEM_string_copy(project_type);
                MEM_free(project_type);
            }
            ifail = AOM_ask_value_string(rev_tag,projectCategory,&project_category);
            if(prog_debug)
            {
                if(project_category == NULL)
                {
                    printf("\nproject category ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nproject category ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(project_category != NULL)
            {
                m_projectCategory = MEM_string_copy(project_category);
                MEM_free(project_category);
            }
            ifail = AOM_ask_value_date(rev_tag,doDate,&do_date);
            ifail = ITK_date_to_string (do_date, &date_string);
            if(prog_debug)
            {
                if(date_string == NULL)
                {
                    printf("\ndate string ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\ndate string ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(date_string != NULL)
            {
                m_reviewDate = MEM_string_copy(date_string);
                MEM_free(date_string);
            }
            ifail = GRM_find_relation_type 	( Relation, &relation_tag );
            if(prog_debug)
            {
                if(relation_tag == NULL)
                if(date_string == NULL)
                {
                    printf("\nTrace Link relation ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nTrace Link relation ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(relation_tag != NULL)
            {
                ifail = GRM_list_primary_objects_only( rev_tag, relation_tag, &count, &secondary_objects );
                if(prog_debug)
                {
                    if(secondary_objects == NULL)
                    {
                        printf("\nprimary object attached to trace link releatiopn ifail is %d %d\n", ifail,__LINE__);
                        fprintf(stderr,"\nprimary object attached to trace link releatiopn ifail and line number is %d %d\n", ifail,__LINE__);
                    }
                }
                if(count >0 && secondary_objects != NULL)
                {
                    int cases = 0;
                    int unapproved_usecases = 0;
                    double percent_usecases = 0.0;
                    int approved_usecases = 0;
                    for( int j = 0; j < count; j++ )
                    {
                        int numParentItemRevs = 0;
                        tag_t *parentItemRevs = NULL;
                        int *levels = 0;
                        ifail = PS_where_used_all ( secondary_objects[j], 1, &numParentItemRevs,  &levels, &parentItemRevs );
                        if(prog_debug)
                        {
                            if(parentItemRevs == NULL)
                            {
                                printf("\nparentItemRevs objects ifail is %d %d\n", ifail,__LINE__);
                                fprintf(stderr,"\nparentItemRevs objects ifail and line number is %d %d\n", ifail,__LINE__);
                            }
                        }
                        if(prog_debug)
                        {
                            printf( "\n where used in second level %d \n", numParentItemRevs );
                        }
                        if ( numParentItemRevs > 0 && parentItemRevs != NULL )
                        {
                            char  	rev_type[ITEM_type_size_c+1];
                            for( int i= 0; i < numParentItemRevs; i++ )
                            {
                                ifail = ITEM_ask_rev_type(parentItemRevs[i], rev_type);
                                if(prog_debug)
                                {
                                    if(rev_type == NULLTAG)
                                        printf("\n rev_type is NULL \n");
                                }
                                if(rev_type != NULL)
                                {

                                    if ( strcmp(rev_type, busUsecase) == 0 )
                                    {
                                        //it = busUseCaseList.begin();
                                        busUseCaseList.push_back(parentItemRevs[i]);
                                        char* Maturity = NULL;
                                        ifail = AOM_ask_value_string( parentItemRevs[i],maturity, &Maturity );
                                        if(prog_debug)
                                        {
                                            if(Maturity == NULL)
                                            {
                                                printf("\nApproved use cases ifail is %d %d\n", ifail,__LINE__);
                                                fprintf(stderr,"\nApproved use cases ifail and line number is %d %d\n", ifail,__LINE__);
                                            }
                                        }
                                        if(Maturity != NULL)
                                        {
                                            if ( strcmp(Maturity, "Approved") == 0 )
                                            {
                                                approved_usecases++;
                                            }
                                            cases++;

                                        }
                                    }
                                }
                            }
                        }
                        MEM_free( parentItemRevs );
                        MEM_free( levels );
                    }
                    unapproved_usecases = cases - approved_usecases;
                    percent_usecases = (approved_usecases*100)/cases;
                    m_approved_usecases= approved_usecases;
                    m_unapproved_usecases = unapproved_usecases;
                    m_percent_usecases_approved = percent_usecases;
                }
            }
            for (it = busUseCaseList.begin(); it!= busUseCaseList.end(); it++)
            {
                int num_as_reqd = 0;
                tag_t *as_reqd = NULL;
                int * levels1 = 0;
                char rev_type[ITEM_type_size_c+1];
                ifail =PS_where_used_all ( *it, 1, &num_as_reqd,  &levels1, &as_reqd );
                if(prog_debug)
                {
                    printf("\n As Required %d\n",num_as_reqd);
                }
                for( int j = 0; j < num_as_reqd; j++ )
                {
                    ifail = ITEM_ask_rev_type(as_reqd[j], rev_type);
                    if(prog_debug)
                    {
                        printf("\n rev_type tag is NULL \n" );
                    }
                    if ( rev_type != NULL )
                    {
                        if ( strcmp(rev_type,asRequiredRevision) == 0 )
                        {
                            tag_t *rl = NULL;
                            int num_rl = 0;
                            int *levels2 = 0;
                            ifail = PS_where_used_all ( as_reqd[j], 1, &num_rl,  &levels2, &rl );
                            if(prog_debug)
                            {
                                if(rl == NULL)
                                {
                                    printf("\nrelease revision ifail is %d %d\n", ifail,__LINE__);
                                    fprintf(stderr,"\nrelease revision ifail and line number is %d %d\n", ifail,__LINE__);
                                }
                            }
                            prop_tag = NULLTAG;
                            char* name= NULL;

                            if ( num_rl > 0 )
                            {
                                ifail = ITEM_ask_rev_type(rl[0], rev_type);
                                if(prog_debug)
                                {
                                    if(rev_type == NULLTAG)
                                        printf("\n release revision type is NULL \n" );
                                }
                                if(rev_type != NULL)
                                {
                                    if ( strcmp(rev_type, ReleaseRevision) == 0 )
                                    {
                                        ifail = AOM_ask_value_string( rl[0],ReleaseName, &name );
                                        if(prog_debug)
                                        {
                                            if(name == NULL)
                                            {
                                                printf("\nrelease revision ifail is %d %d\n", ifail,__LINE__);
                                                fprintf(stderr,"\nrelease revision ifail and line number is %d %d\n", ifail,__LINE__);
                                            }
                                        }
                                        if ( name != NULL)
                                        {
                                            m_releaseName = MEM_string_copy(name);
                                            flag = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

            }
            MEM_free( secondary_objects );
            char *value = NULL;
            ifail = AOM_ask_value_string(rev_tag,portfolioElement,&value);
            if(prog_debug)
            {
                if(value == NULL)
                {
                    printf("\nportfolio Element namen ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nportfolio Element name ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(value != NULL)
            {
                m_portfolioValue=MEM_string_copy(value);
            }
            int numParentItemRevs = 0;
            tag_t *parentItemRevs = NULL;
            int *levels = 0;
            ifail = PS_where_used_all ( rev_tag, 2, &numParentItemRevs,  &levels, &parentItemRevs );
            if(prog_debug)
            {
                if(parentItemRevs == NULL)
                {
                    printf("\nsecond level revisions ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nsecond level revisions ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if ( numParentItemRevs > 0 && parentItemRevs != NULL )
            {
                char  	rev_type[ITEM_type_size_c+1];
                char *value1 = NULL;
                for( int i= 0; i < numParentItemRevs; i++ )
                {
                    ifail = ITEM_ask_rev_type(parentItemRevs[i], rev_type);
                    if(prog_debug)
                    {
                        if(rev_type == NULLTAG)
                        {
                            printf("\nrelease revison ifail is %d %d\n", ifail,__LINE__);
                            fprintf(stderr,"\nrelease revison ifail and line number is %d %d\n", ifail,__LINE__);
                        }
                    }
                    if(rev_type != NULL)
                    {
                        if ( strcmp(rev_type, ReleaseRevision) == 0 )
                        {
                            ifail = AOM_ask_value_string(parentItemRevs[i],"object_name",&value1);
                            if(prog_debug)
                            {
                                if(value1 == NULL)
                                {
                                    printf("\nobject name of release revision ifail is %d %d\n", ifail,__LINE__);
                                    fprintf(stderr,"\nobject name of release revision ifail and line number is %d %d\n", ifail,__LINE__);
                                }
                            }
                            if(value1 != NULL)
                            {
                                m_POR = MEM_string_copy(value1);
                            }
                        }
                    }
                }
                MEM_free(value1);
                MEM_free(parentItemRevs);
                MEM_free(levels);
            }
            ifail = get_pbb_info(rev_tag);
            char *uid = NULL;
            string url;
            ITK__convert_tag_to_uid( item_tag, &uid );
            if ( uid != NULL )
            {
                url = MEM_string_copy("http://usslslabsdlc04:7001/bpsw/webclient?argument=");
                m_url = url.append( uid );
                MEM_free( uid );
            }
        }
    }
    return ifail;
}
/*
 * This mehod will get dependent projects of current development project item revision.
 * This method also get the child items and its revisions attached to the development project item revision
 */
int PSR_Project::get_pbb_info(tag_t rev_tag )
{
    int ifail = ITK_ok;
    int dep1_count = 0;
    int count1 = 0;
    tag_t *dep1_proj = NULL;
    int dep2_count = 0;
    int count2 = 0;
    tag_t *dep2_proj = NULL,
    relation_tag = NULLTAG,
    prop_tag = NULLTAG;
    char  rev_type[ITEM_type_size_c+1];
    ifail = GRM_find_relation_type ( Relation, &relation_tag );
    if ( relation_tag != NULLTAG )
    {
        ifail = GRM_list_primary_objects_only( rev_tag, relation_tag, &count1, &dep1_proj );
        if(prog_debug)
        {
            if(dep1_proj == NULL)
            {
                printf("\ndependent projects are null ifail is %d %d\n", ifail,__LINE__);
                fprintf(stderr,"\ndependent projects are null ifail and line number is %d %d\n", ifail,__LINE__);
            }
        }
        if( count1 > 0 && dep1_proj != NULL )
        {
            for( int i= 0; i < count1; i++ )
            {
                char *dep_proj_id = NULL;
                ifail = ITEM_ask_rev_type(dep1_proj[i], rev_type);

                if ( strcmp(rev_type, devProject) == 0 )
                {
                    ifail = PROP_ask_property_by_name ( dep1_proj[i], projectId, &prop_tag );
                    if(prop_tag != NULLTAG )
                    {
                        ifail = PROP_ask_value_string ( prop_tag, &dep_proj_id );
                        if ( dep_proj_id != NULL )
                        {
                            this->dep1_proj.push_back(dep_proj_id);
                            dep1_count++;

                        }
                    }
                }
            }
        }
        MEM_free( dep1_proj );
        ifail = GRM_list_secondary_objects_only( rev_tag, relation_tag, &count2, &dep2_proj );
        if(prog_debug)
        {
            if(dep2_proj == NULL)
            {
                printf("\ndependent projects are null ifail is %d %d\n", ifail,__LINE__);
                fprintf(stderr,"\ndependent projects are null ifail and line number is %d %d\n", ifail,__LINE__);
            }
        }
        if( count2 > 0 && dep2_proj != NULL )
        {
            for( int i= 0; i < count2; i++ )
            {
                char *dep_proj_id = NULL;
                ifail = ITEM_ask_rev_type(dep2_proj[i], rev_type);
                if ( strcmp(rev_type, devProject) == 0 )
                {
                    ifail = PROP_ask_property_by_name ( dep2_proj[i], projectId, &prop_tag );
                    if(prop_tag != NULL)
                    {
                        ifail = PROP_ask_value_string ( prop_tag, &dep_proj_id );
                        if ( dep_proj_id != NULL )
                        {
                            this->dep2_proj.push_back(dep_proj_id);
                            dep2_count++;
                        }
                    }
                }
            }
        }
        MEM_free( dep2_proj );
    }
    this->dep1_count = dep1_count;
    this->dep2_count = dep2_count;
    int tds_ctr = 0;
    int bvr_count = 0;
    tag_t *bvrs = NULL;
    tag_t bom_view = NULLTAG;
    tag_t child_rev_tag = NULLTAG;
    this->specs_info = (proj_info *)MEM_alloc( sizeof ( proj_info ) );
    if (specs_info)
    {
        memset( specs_info, 0, sizeof ( proj_info ) );
    }
    if( rev_tag != NULL)
    {
        ifail = ITEM_rev_list_bom_view_revs ( rev_tag, &bvr_count, &bvrs );
        if(prog_debug)
        {
            if( bvr_count == 0 && bvrs != NULL )
            {
                printf("\nbvr count null ifail is %d %d\n", ifail,__LINE__);
                fprintf(stderr,"\nbvr count null ifail and line number is %d %d\n", ifail,__LINE__);
            }
        }
        tag_t child_item,child_bv = NULLTAG;
        int  n_occurrences = 0;
        tag_t   *occurrences = NULL;
        char  item_type[ITEM_type_size_c+1];
        if(bvr_count > 0)
        {
            for (int jj = 0; jj < bvr_count; jj++)
            {
                ifail = PS_list_occurrences_of_bvr( bvrs[jj],&n_occurrences, &occurrences);
                if(prog_debug)
                {
                    if( n_occurrences == 0 && occurrences != NULL )
                    {
                        printf("\nchild line ifail is %d %d\n", ifail,__LINE__);
                        fprintf(stderr,"\nchild line ifail and line number is %d %d\n", ifail,__LINE__);
                    }
                }
                for (int jnx = 0; jnx < n_occurrences; jnx++)
                {
                    ifail = PS_ask_occurrence_child( bvrs[jj], occurrences[jnx], &child_item, &child_bv );
                    if(prog_debug)
                    {
                        if( child_item == NULLTAG )
                        {
                            printf("\nchild line ifail is %d %d\n", ifail,__LINE__);
                            fprintf(stderr,"\nchild line ifail and line number is %d %d\n", ifail,__LINE__);
                        }
                    }
                    if(child_item != NULLTAG)
                    {
                        ifail = ITEM_ask_type ( child_item,	item_type );
                        ifail = ITEM_ask_latest_rev( child_item, &child_rev_tag );
                    }
                    if( prog_debug )
                    {
                        printf("item has multiple revision \n");
                        printf("\n item type %s\n",item_type);
                        int count = 0;
                        tag_t *rev_list = NULL;
                        ITEM_list_all_revs  ( child_item,&count,&rev_list );
                        if(count > 0)
                        {
                            printf("item revisions count %d\n",count);
                        }
                        MEM_free(rev_list);
                    }
                    if ( child_rev_tag != NULLTAG && item_type != NULLTAG )
                    {
                        bool is_incremented = false;
                        ifail = get_proj_info(child_rev_tag, item_type, tds_ctr, &is_incremented );
                        if(is_incremented)
                        {
                            tds_ctr++;
                        }
                    }
                }
                MEM_free( occurrences );
                MEM_free( bvrs );
            }
            if ( tds_ctr )
                specs_info->num_tds_docs = tds_ctr;
        }
    }
    return ifail;
}
/*
 * This method validate the item types of document(SRS,FS,SDD,PTP..etc) attached to devlopment project item revision.
 * To get work flow details attached to each document from this method get_spec_info.
 */

int PSR_Project::get_proj_info( tag_t child_rev_tag, char item_type[ITEM_type_size_c+1], int tds_ctr, bool *is_incremented )
{
    int ifail = ITK_ok;
    if (strcmp(item_type, docSDD) == 0)
        ifail = specs_info->sdd_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docSRS) == 0)
        ifail = specs_info->srs_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docFS) == 0)
        ifail = specs_info->fs_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docPDR) == 0)
        ifail = specs_info->pdr_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docPTP) == 0)
        ifail = specs_info->ptp_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docSCS) == 0)
        ifail = specs_info->scs_info.get_spec_info(child_rev_tag);
    if(strcmp(item_type, docBOE) == 0)
        ifail = specs_info->boe_info.get_spec_info(child_rev_tag);
    if (strcmp(item_type, docTDS) == 0)
    {
        *is_incremented = true;
        ifail = specs_info->tds_info[tds_ctr].get_spec_info(child_rev_tag);
    }

    return ifail;
}
/*
 * This method get the release status,reviewers name,decisions,decision dates,due date
 * of different docs(SRS,FS,SDD,...etc) attached to development project item revision
 */
int spec::get_spec_info( tag_t revtag )
{
    int ifail = ITK_ok;
    tag_t *process_stage = NULLTAG;
    tag_t prop_tag = NULLTAG;
    EPM_task_type_t task_Type = EPMReviewTask;
    int task_num= 0;
    int count = 0;
    tag_t job = NULLTAG;
    char        release_level_name[WSO_name_size_c+1];
    tag_t       *signoffs=NULL;
    tag_t       *release_status_tag = NULL;
    int         status_count = 0;
    int jnx = 0;
    char * szDate = NULL;
    char personname[SA_name_size_c+1]  = "";
    tag_t user_tag = NULLTAG;
    int n_signoffs = 0;
    EPM_decision_t *decisions = NULL;
    char **signers = NULL;
    date_t *sign_off_dates = NULL;
    logical is_late = false;
    date_t dueDate;
    tag_t * msg_task = NULL;
    tag_t *status_list = NULL;
    tag_t root_task = NULLTAG;
    string release_status;
    char relStatName[WSO_name_size_c + 1];
    int rel_count = 0;

    m_found = true;
    ifail = WSOM_ask_release_status_list (revtag, &rel_count, &status_list);
    if(prog_debug)
    {
        if( rel_count == 0 )
        {
            printf("\nrelease statuses for this document ifail is %d %d\n", ifail,__LINE__);
            fprintf(stderr,"\nrelease statuses for this document ifail and line number is %d %d\n", ifail,__LINE__);
        }
    }
    if( rel_count>0 && status_list != NULL )
    {
        for(int knx=0;knx<rel_count;knx++)
        {
            ifail = CR_ask_release_status_type(status_list[knx],relStatName);
            if(prog_debug)
            {
                if( relStatName == NULL )
                {
                    printf("\nrelease statuses name for this document ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\nrelease statuses name for this document ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
            if(relStatName != NULL)
            {
                release_status.append(relStatName);
                if(knx != rel_count-1 )
                    release_status.append(",");
            }
        }
        m_status=release_status;
        MEM_free(status_list);
    }
    /*
       1) To get the details of tasks attached to process stage list from the query of item property(process_stageList).
       2) Process stage list contains different tasks.
       3) Any one of task to get the root task.This root task is same for all tasks.
       4) From root task to get job.this job is same for all tasks.
       5) Review task contains all signoff details so get all review tasks attached to this job.
       6) for each review task will get all signoff details like reviwer name,decision and date.
       7) Based on the reviewer decision will populate pending signoffs or reject signoffs.
       all signoff details will be stored in vector of reviwer_info structure
    */

    ifail = PROP_ask_property_by_name ( revtag, process_stageList, &prop_tag );
    if(prop_tag != NULL)
    {
        ifail = PROP_ask_value_tags ( prop_tag, &count, &process_stage );
    }
    if( prog_debug )
    {
        if(count > 0)
        {
            printf("process_stage count %d\n",count);
            fprintf(stderr,"\nprocess_stage is %d %d\n", count,__LINE__);
        }
        else
            printf("process_stage list is null \n");
    }
    if (process_stage != NULL)
    {
        ifail = EPM_ask_root_task(process_stage[0],&root_task);
        if(prog_debug)
        {
            if( root_task == NULLTAG )
            {
                printf("\nroot task for all tasks or jobs attached to docRevision ifail is %d %d\n", ifail,__LINE__);
                fprintf(stderr,"\nroot task for all tasks or jobs attached to docRevision ifail and line number is %d %d\n", ifail,__LINE__);
            }
        }
        MEM_free( process_stage );
        if(root_task != NULLTAG)
        {
            ifail = EPM_ask_job(root_task, &job);
            if(prog_debug)
            {
                if( job == NULLTAG )
                {
                    printf("\njob for root task ifail is %d %d\n", ifail,__LINE__);
                    fprintf(stderr,"\njob for root task ifail and line number is %d %d\n", ifail,__LINE__);
                }
            }
        }
        if(job != NULLTAG)
        {
            ifail = EPM_get_type_tasks(job,task_Type,&task_num, &msg_task );
            ifail = EPM_ask_task_late(msg_task[0], &is_late );
            if (is_late != false )
            {
                m_is_late = is_late;
            }
            if( prog_debug )
            {
                printf("\n  no of review tasks attached to job %d\n",task_num);
                fprintf(stderr,"\n no of review tasks attached to job is %d %d\n", task_num,__LINE__);
            }

            ifail = EPM_ask_task_due_date( msg_task[0], &dueDate);
            due_date = dueDate;
            if(task_num >0 && msg_task != NULL)
            {
                for ( int i = 0; i < task_num; i++)
                {
                    tag_t classId = null_tag;
                    char *class_name = 0;
                    tag_t prop_tag = null_tag;
                    char *prop_name = 0;
                    ifail = EPM_ask_review_task_name( msg_task[i], release_level_name );
                    if(release_level_name != NULL)
                    {
                        ifail = CR_ask_reviewers ( job, release_level_name, &n_signoffs, &signoffs );
                        if( prog_debug )
                        {
                            printf("\n signoff details for review task is  %d\n",n_signoffs);
                            fprintf(stderr,"\n signoff details for review task is is %d %d\n", n_signoffs,__LINE__);
                        }
                    }
                    num_reviewers = num_reviewers + n_signoffs;
                    if ( n_signoffs >0 )
                    {
                        reviewers = (reviewer_info *)MEM_alloc( sizeof ( reviewer_info ) );
                        if (  reviewers )
                        {
                            memset( reviewers, 0, sizeof ( reviewer_info ) );
                            char                     review_comments[CR_comment_size_c+1];
                            date_t                   decision_date = NULLDATE;
                            char                    *date_string=NULL;
                            CR_signoff_decision_t    decision = CR_no_decision;
                            const char              *decision_string=NULL;
                            char                     reviewerName[SA_person_name_size_c+1];
                            char                     userid[SA_user_size_c + 1];
                            tag_t                    member = NULLTAG;
                            tag_t                    user_value = NULLTAG;
                            for ( int j=0; j<n_signoffs; j++ )
                            {
                                ifail = POM_class_of_instance( signoffs[j], &classId );
                                if(classId != NULL)
                                {
                                    ifail = POM_name_of_class (classId, &class_name);
                                }
                                if(class_name != NULL)
                                {
                                    if (strcmp(class_name, "ResourcePool") == 0)
                                    {
                                        /*
                                        * its a resourcepool assignment put the resourcepool name in the
                                        * user name string as it will be appropriate for the report
                                        */
                                        member = signoffs[j];
                                        ifail = PROP_ask_property_by_name( member, "resourcepool_name", &prop_tag );
                                        if(prop_tag != NULL)
                                        {
                                            ifail = PROP_ask_value_string( prop_tag, &prop_name );
                                        }
                                        ifail = SA_find_user( prop_name,&user_value );
                                        ifail = SA_ask_user_person_name( user_value,reviewerName );
                                    }
                                    else
                                    {
                                        /* it's a groupmember assignment, we can get the assigned users details */
                                        ifail = SA_ask_groupmember_user(signoffs[j],&member);

                                        /** getting the corresponding user NAME **/
                                        if( prog_debug )
                                        {
                                            if(member == NULLTAG)
                                                printf("\n groupmember for this signoff is null \n ");
                                        }
                                        if(member != NULLTAG)
                                        {
                                            ifail = SA_ask_user_identifier(member,userid);
                                            ifail = SA_find_user( userid,&user_value );
                                            ifail = SA_ask_user_person_name( user_value,reviewerName );
                                        }
                                    }
                                }
                                reviewers->name = MEM_string_copy(reviewerName);
                                ifail = CR_ask_decision( job, release_level_name,member,&decision, review_comments, &decision_date );
                                if ( decision == CR_approve_decision )            decision_string = "Approved";
                                else if ( decision == CR_reject_decision )        decision_string = "Rejected";
                                else if ( decision == CR_no_decision )            decision_string = "No Decision";
                                else   decision_string = "invalid decision type";
                                reviewers->decision = MEM_string_copy(decision_string);
                                ifail = ITK_date_to_string (decision_date, &date_string );
                                if( prog_debug )
                                {
                                    printf("\n fail to insert data into vector \n ");
                                    fprintf(stderr,"\n fail to insert data into vector %d\n",__LINE__);
                                }
                                this->reviewers1.push_back(*reviewers);
                                reviewers++;
                            }
                            MEM_free(class_name);
                        }
                    }
                    MEM_free ( signoffs );
                }
            }
        }
        MEM_free( msg_task );
    }
    /*
       1) To get the details of tasks or jobs attached to release status list from the simple query of item revision property(release_statusList).
       2) Release status List contains different tasks or jobs
       3) For all jobs contains same signoff details.
       4) To get signoff details like reviwer name,decision and date from any of the job.
       5) Based on the reviewer decision will populate pending signoffs or reject signoffs
       all signoff details will be stored in vector of reviwer_info structure
    */
    else
    {
        ifail = PROP_ask_property_by_name ( revtag, release_statusList, &prop_tag );
        if(prop_tag != NULL)
        {
            ifail = PROP_ask_value_tags( prop_tag, &status_count, &release_status_tag );
        }
        if( prog_debug )
        {
            printf("\n release status list %d\n",status_count);
            fprintf(stderr,"\n release status list is is %d %d\n", status_count,__LINE__);
        }
        signers = (char **)MEM_alloc( status_count * sizeof(char*) );
        if(status_count>0)
        {
            ifail = EPM_ask_signoff_details( revtag, release_status_tag[0], &n_signoffs, &decisions, &signers, &sign_off_dates);
            if( prog_debug )
            {
                printf("\n no of signoffs %d\n",n_signoffs);
                fprintf(stderr,"\n no of signoffs %d\n",__LINE__);
            }
            MEM_free(release_status_tag);
            if (n_signoffs > 0)
            {
                num_reviewers = n_signoffs ;
                reviewers = (reviewer_info *)MEM_alloc( sizeof ( reviewer_info ) );
                if ( reviewers )
                {
                    memset(reviewers, 0, sizeof ( reviewer_info ) );
                    num_reviewers = n_signoffs ;
                    for (jnx = 0; jnx < n_signoffs; jnx++)
                    {
                        if (decisions[jnx] == EPM_nogo)
                        {
                            reviewers->decision = MEM_string_copy("Rejected");
                        }
                        else if( decisions[jnx] == EPM_undecided )
                        {
                            reviewers->decision = MEM_string_copy("No Decision");
                        }
                        else if( decisions[jnx] == EPM_go )
                        {
                            reviewers->decision = MEM_string_copy("Approved");
                        }
                        else
                        {
                            reviewers->decision=MEM_string_copy("invalid decision type");
                        }
                        ifail = ITK_date_to_string (sign_off_dates[jnx], &szDate );
                        signoff_date = sign_off_dates[jnx];
                        ifail = SA_find_user( signers[jnx],&user_tag );
                        ifail = SA_ask_user_person_name( user_tag,personname );
                        reviewers->name = MEM_string_copy(personname);
                        if( prog_debug )
                        {
                            printf("\n fail to insert data into vector \n ");
                            fprintf(stderr,"\n fail to insert data into vector %d\n",__LINE__);
                        }
                        this->reviewers1.push_back(*reviewers);
                        reviewers++;
                    }
                }
            }
        }
    }
    MEM_free(decisions);
    MEM_free(szDate);
    MEM_free(sign_off_dates);
    tag_t *attchments = NULL;
    int num = 0;
    ifail = GRM_find_relation_type( "TC_Attaches", &prop_tag );
    ifail = GRM_list_secondary_objects_only (revtag, prop_tag, &num, &attchments );
    ifail = PROP_ask_value_tags( prop_tag, &num, &attchments );
    if ( num >0 )
    {
        char *uid = NULL;
        string link;
        POM_tag_to_uid( attchments[0], &uid );
        if ( uid != NULL )
        {
            link.append("http://usslslabsdlc04:7001/bpsw/webclient?argument=");
            link.append(uid);
            m_link = link;
            MEM_free( uid );
        }
    }
    MEM_free( attchments );
    return ifail;
}

static void display_pbpinfo(PSR_Project * s1)
{

    string fs_review_print_info;
    string srs_review_print_info;
    string boe_review_print_info;
    string sdd_review_print_info;
    string ptp_review_print_info;
    string pdr_review_print_info;
    string tds_review_print_info;
    //list <string> tdsList;
    //list <string>::iterator it;
    string scs_review_print_info;
    string fs_status;
    string srs_status;
    string ptp_status;
    string sdd_status;
    string tds_status;
    string boe_status;
    string pdr_status;
    string scs_status;
    int fs_count = 0;
    int srs_count = 0;
    int boe_count = 0;
    int ptp_count = 0;
    int scs_count = 0;
    int tds_cnt = 0;
    int pdr_count = 0;
    int sdd_count = 0;
    if(s1->specs_info->fs_info.isFound())
    {
        if (strcmp (s1->specs_info->fs_info.getStatus().c_str(), "FS Review Complete,FS Approved")==0)
            fs_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->fs_info.getStatus().c_str(),"FS in Review,Examination in Progress")==0)
            fs_status = MEM_string_copy("In Review");
        else
            fs_status = MEM_string_copy("In Progress");
    }
    else
    {
        fs_status = MEM_string_copy("Not Found");
    }
    for( int inx = 0; inx < s1->specs_info->fs_info.getReviewers().size(); ++inx)
    {
        reviewer_info reviewers2 = s1->specs_info->fs_info.getReviewers().at(inx);
        if(strcmp(reviewers2.decision,"Rejected")== 0)
        {
            fs_count++;
			if (fs_count == 1)
                fs_review_print_info.assign("Rejected By:");

			fs_review_print_info.append(reviewers2.name);
            fs_review_print_info.append(";");
        }
        else if (strcmp(reviewers2.decision,"No Decision")== 0)
		{
			fs_count++;
			if (fs_count == 1)
				fs_review_print_info.assign("Pending Signoffs:");

			fs_review_print_info.append(reviewers2.name);
            fs_review_print_info.append(";");
        }
    }
    if(s1->specs_info->srs_info.isFound())
    {
        if (strcmp (s1->specs_info->srs_info.getStatus().c_str(), "SRS Review Complete,SRS Approved")==0)
            srs_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->srs_info.getStatus().c_str(),"SRS in Review,Examination in Progress")==0)
            srs_status = MEM_string_copy("In Review");
        else
            srs_status = MEM_string_copy("In Progress");
    }
    else
    {
        srs_status = MEM_string_copy("Not Found");
    }
    for( int jnx = 0; jnx < s1->specs_info->srs_info.getNumReviewrs(); ++jnx )
    {
        reviewer_info reviewers3 = s1->specs_info->srs_info.getReviewers().at(jnx);
        if(strcmp(reviewers3.decision,"Rejected")== 0)
        {
            srs_count++;
			if (srs_count == 1)
                srs_review_print_info.assign("Rejected By:");

			srs_review_print_info.append(reviewers3.name);
            srs_review_print_info.append(";");
        }
        else if (strcmp(reviewers3.decision,"No Decision")== 0)
		{
			srs_count++;
			if (srs_count == 1)
				srs_review_print_info.assign("Pending Signoffs:");

			srs_review_print_info.append(reviewers3.name);
            srs_review_print_info.append(";");
        }
    }
    if(s1->specs_info->sdd_info.isFound())
    {
        if (strcmp (s1->specs_info->sdd_info.getStatus().c_str(), "SDD Review Complete,SDD Approved")==0)
            sdd_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->sdd_info.getStatus().c_str(),"SDD in Review,Examination in Progress")==0)
            sdd_status = MEM_string_copy("In Review");
        else
            sdd_status = MEM_string_copy("In Progress");
    }
    else
    {
        sdd_status = MEM_string_copy("Not Found");
    }
    for( int knx = 0; knx < s1->specs_info->sdd_info.getNumReviewrs(); ++knx )
    {
        reviewer_info reviewers5 = s1->specs_info->sdd_info.getReviewers().at(knx);
        if(strcmp(reviewers5.decision,"Rejected")== 0)
        {
            sdd_count++;
			if (sdd_count == 1)
                sdd_review_print_info.assign("Rejected By:");

			sdd_review_print_info.append(reviewers5.name);
            sdd_review_print_info.append(";");
        }
        else if (strcmp(reviewers5.decision,"No Decision")== 0)
		{
			sdd_count++;
			if (sdd_count == 1)
				sdd_review_print_info.assign("Pending Signoffs:");

			sdd_review_print_info.append(reviewers5.name);
            sdd_review_print_info.append(";");
        }
    }
    if(s1->specs_info->ptp_info.isFound())
    {
        if (strcmp (s1->specs_info->ptp_info.getStatus().c_str(), "PTP Review Complete,PTP Approved")==0)
            ptp_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->ptp_info.getStatus().c_str(),"PTP in Review,Examination in Progress")==0)
            ptp_status = MEM_string_copy("In Review");
        else
            ptp_status = MEM_string_copy("In Progress");
    }
    else
    {
        ptp_status = MEM_string_copy("Not Found");
    }
    for( int knx = 0; knx < s1->specs_info->ptp_info.getNumReviewrs(); ++knx )
    {
        reviewer_info reviewers6 = s1->specs_info->ptp_info.getReviewers().at(knx);
        if(strcmp(reviewers6.decision,"Rejected")== 0)
        {
            ptp_count++;
			if (ptp_count == 1)
                ptp_review_print_info.assign("Rejected By:");

			ptp_review_print_info.append(reviewers6.name);
            ptp_review_print_info.append(";");
        }
        else if (strcmp(reviewers6.decision,"No Decision")== 0)
		{
			ptp_count++;
			if (ptp_count == 1)
				ptp_review_print_info.assign("Pending Signoffs:");

			ptp_review_print_info.append(reviewers6.name);
            ptp_review_print_info.append(";");
        }
    }
    if(s1->specs_info->pdr_info.isFound())
    {
        if (strcmp (s1->specs_info->pdr_info.getStatus().c_str(), "PDR Review Complete,PDR Approved")==0)
            pdr_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->pdr_info.getStatus().c_str(),"PDR in Review,Examination in Progress")==0)
            pdr_status = MEM_string_copy("In Review");
        else
            pdr_status = MEM_string_copy("In Progress");
    }
    else
    {
        pdr_status = MEM_string_copy("Not Found");
    }
    for( int knx = 0; knx < s1->specs_info->pdr_info.getNumReviewrs(); ++knx )
    {
        reviewer_info reviewers8 = s1->specs_info->pdr_info.getReviewers().at(knx);
        if(strcmp(reviewers8.decision,"Rejected")== 0)
        {
            pdr_count++;
			if (pdr_count == 1)
                pdr_review_print_info.assign("Rejected By:");

			pdr_review_print_info.append(reviewers8.name);
            pdr_review_print_info.append(";");
        }
        else if (strcmp(reviewers8.decision,"No Decision")== 0)
		{
			pdr_count++;
			if (pdr_count == 1)
				pdr_review_print_info.assign("Pending Signoffs:");

			pdr_review_print_info.append(reviewers8.name);
            pdr_review_print_info.append(";");
        }
    }
    int tds_count = s1->specs_info->num_tds_docs;
    if(tds_count>0)
    {
        for(int inx =0;inx < tds_count; inx++)
        {
            if(s1->specs_info->tds_info[inx].isFound())
            {
                if (strcmp (s1->specs_info->tds_info[inx].getStatus().c_str(), "TDS Review Complete,TDS Approved")==0)
                    tds_status = MEM_string_copy("Approved");
                else if(strcmp(s1->specs_info->tds_info[inx].getStatus().c_str(),"TDS in Review,Examination in Progress")==0)
                    tds_status = MEM_string_copy("In Review");
                else
                    tds_status = MEM_string_copy("In Progress");
                //tdsList.push_back(tds_status);
            }
        }
    }
    else
    {
        tds_status = MEM_string_copy("Not Found");
    }
    if(tds_count>0)
    {
        for(int i =0; i<tds_count; i++)
        {
            for( int j = 0; j < s1->specs_info->tds_info[i].getNumReviewrs(); ++j )
            {
                reviewer_info reviewers9 = s1->specs_info->tds_info[i].getReviewers().at(j);
                if(strcmp(reviewers9.decision,"Rejected")== 0)
                {
                    tds_cnt++;
                    if (tds_cnt == 1)
                        tds_review_print_info.assign("Rejected By:");

                    tds_review_print_info.append(reviewers9.name);
                    tds_review_print_info.append(";");
                }
                else if (strcmp(reviewers9.decision,"No Decision")== 0)
                {
                    tds_cnt++;
                    if (tds_cnt == 1)
                        tds_review_print_info.assign("Pending Signoffs:");

                    tds_review_print_info.append(reviewers9.name);
                    tds_review_print_info.append(";");
                }
            }
        }
    }
    if(s1->specs_info->boe_info.isFound())
    {
        if (strcmp (s1->specs_info->boe_info.getStatus().c_str(), "BOE Review Complete,BOE Approved")==0)
            boe_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->boe_info.getStatus().c_str(),"BOE in Review,Examination in Progress")==0)
            boe_status = MEM_string_copy("In Review");
        else
            boe_status = MEM_string_copy("In Progress");
    }
    else
    {
        boe_status = MEM_string_copy("Not Found");
    }
    for( int knx = 0; knx < s1->specs_info->boe_info.getNumReviewrs(); ++knx )
    {
        reviewer_info reviewers4 = s1->specs_info->boe_info.getReviewers().at(knx);
        if(strcmp(reviewers4.decision,"Rejected")== 0)
        {
            boe_count++;
			if (boe_count == 1)
                boe_review_print_info.assign("Rejected By:");

			boe_review_print_info.append(reviewers4.name);
            boe_review_print_info.append(";");
        }
        else if (strcmp(reviewers4.decision,"No Decision")== 0)
		{
			boe_count++;
			if (boe_count == 1)
				boe_review_print_info.assign("Pending Signoffs:");

			boe_review_print_info.append(reviewers4.name);
            boe_review_print_info.append(";");
        }
    }
    if(s1->specs_info->scs_info.isFound())
    {
        if (strcmp (s1->specs_info->scs_info.getStatus().c_str(), "SCS Review Complete,SCS Approved")==0)
            scs_status = MEM_string_copy("Approved");
        else if(strcmp(s1->specs_info->scs_info.getStatus().c_str(),"SCS in Review,Examination in Progress")==0)
            scs_status = MEM_string_copy("In Review");
        else
            scs_status = MEM_string_copy("In Progress");
    }
    else
    {
        scs_status = MEM_string_copy("Not Found");
    }
    for( int knx = 0; knx < s1->specs_info->scs_info.getNumReviewrs(); ++knx )
    {
        reviewer_info reviewers7 = s1->specs_info->scs_info.getReviewers().at(knx);
        if(strcmp(reviewers7.decision,"Rejected")== 0)
        {
            scs_count++;
			if (scs_count == 1)
                scs_review_print_info.assign("Rejected By:");

			scs_review_print_info.append(reviewers7.name);
            scs_review_print_info.append(";");
        }
        else if (strcmp(reviewers7.decision,"No Decision")== 0)
		{
			scs_count++;
			if (scs_count == 1)
				scs_review_print_info.assign("Pending Signoffs:");

			scs_review_print_info.append(reviewers7.name);
            scs_review_print_info.append(";");
        }
    }
    int i, err;
    FILE *fp;

    fp = fopen(filename,"wt");
    if (fp == NULL)
    {
        fprintf(stderr,"Unable to write to output file '%s'.\n",filename);
        //logout();
        exit(1);
    }
    fprintf(fp,"\nproject id: %s\n", s1->getId().c_str());
    fprintf(fp,"\nproject name: %s\n", s1->getName().c_str());
    fprintf(fp,"\nproject desc:  %s\n", s1->getDesc().c_str());
    fprintf(fp,"\n owner:  %s\n", s1->getOwner().c_str());
    fprintf(fp,"\n PRE data: %s\n",s1->getPRE().c_str());
    fprintf(fp,"\n POR data: %s\n",s1->getPOR().c_str());
    fprintf(fp,"\n Dev Manager name: %s \n",s1->getDevManager().c_str());
    fprintf(fp,"\n Dev Manager Email: %s \n",s1->getDevManagerEmail().c_str());
    fprintf(fp,"\n Domain Manager name: %s \n",s1->getDomManager().c_str());
    fprintf(fp,"\n Domain Manager Email: %s \n",s1->getDomManagerEmail().c_str());
    fprintf(fp,"\n QA Lead name: %s \n",s1->getQALead().c_str());
    fprintf(fp,"\n QA Lead Email: %s \n",s1->getQALeadEmail().c_str());
    fprintf(fp,"\n tDoc Lead name: %s \n",s1->gettDocLead().c_str());
    fprintf(fp,"\n tDoc Lead Email: %s \n",s1->gettDocLeadEmail().c_str());
    fprintf(fp,"\n project satus list: %s \n",s1->getProjectList().c_str());
    fprintf(fp,"\n Product Manager name: %s \n",s1->getprodManager().c_str());
    fprintf(fp,"\n Product Manager Email: %s \n",s1->getprodManagerEmail().c_str());
    fprintf(fp,"\n url: %s \n",s1->getUrl().c_str());
    fprintf(fp,"\n Project Architech name: %s \n",s1->getprimArch().c_str());
    fprintf(fp,"\n Project Status: %s \n",s1->getProjectStatus().c_str());
    fprintf(fp,"\n Project Category: %s \n",s1->getProjectCategory().c_str());
    fprintf(fp,"\n Project Type: %s \n",s1->getProjectType().c_str());
    fprintf(fp,"\n Review Date: %s \n",s1->getreviewDate().c_str());
    fprintf(fp,"\n Man Month: %f \n",s1->getManMonth());
    fprintf(fp,"\n Approved useCases: %d \n",s1->getapproved_usecases());
    fprintf(fp,"\n Unapproved useCases: %d \n",s1->getunapproved_usecases());
    fprintf(fp,"\n percentage usecases approved: %f \n",s1->getpercentUsecasesApproved());
    fprintf(fp,"\n Release Name: %s \n",s1->getreleaseName().c_str());
    fprintf(fp,"\n No of projs that this project depends on: %d\n", s1->getDep1_count());
    for( int i = 0; i < (int)s1->getDep1_proj().size(); i++ )
        fprintf(fp,"%s,\t", s1->getDep1_proj()[i]);
    fprintf(fp,"\n Projects that depend on this project: %d\n", s1->getDep2_count());
    for( int i = 0; i < (int)s1->getDep2_proj().size(); i++ )
    fprintf(fp,"%s,\t", s1->getDep2_proj()[i]);
    fprintf( fp,"\n fs status: %s",fs_status.c_str());
    fprintf( fp,"\n fs Signoff Due Date: %02d%02d%04d",s1->specs_info->fs_info.getDueDate().month+1,s1->specs_info->fs_info.getDueDate().day,s1->specs_info->fs_info.getDueDate().year);
    fprintf(fp,"\n %s \n",fs_review_print_info.c_str());
    fprintf(fp,"\n link for FS: %s",s1->specs_info->fs_info.getLink().c_str());
    fprintf( fp,"\n srs status: %s",srs_status.c_str());
    fprintf( fp,"\n srs Signoff Due Date: %02d%02d%04d",s1->specs_info->srs_info.getDueDate().month+1,s1->specs_info->srs_info.getDueDate().day,s1->specs_info->srs_info.getDueDate().year);
    fprintf(fp,"\n %s \n",srs_review_print_info.c_str());
    fprintf(fp,"\n link for SRS: %s",s1->specs_info->srs_info.getLink().c_str());
    fprintf( fp,"\n sdd status: %s",sdd_status.c_str());
    fprintf( fp,"\n sdd Signoff Due Date: %02d%02d%04d",s1->specs_info->sdd_info.getDueDate().month+1,s1->specs_info->sdd_info.getDueDate().day,s1->specs_info->sdd_info.getDueDate().year);
    fprintf(fp,"\n %s \n",sdd_review_print_info.c_str());
    fprintf(fp,"\n link for SDD: %s",s1->specs_info->sdd_info.getLink().c_str());
    fprintf( fp,"\n sdd status: %s",scs_status.c_str());
    fprintf( fp,"\n scs Signoff Due Date: %02d%02d%04d",s1->specs_info->scs_info.getDueDate().month+1,s1->specs_info->scs_info.getDueDate().day,s1->specs_info->scs_info.getDueDate().year);
    fprintf(fp,"\n %s \n",scs_review_print_info.c_str());
    fprintf(fp,"\n link for SCS: %s",s1->specs_info->scs_info.getLink().c_str());
    fprintf( fp,"\n ptp status: %s",ptp_status.c_str());
    fprintf( fp,"\n fs Signoff Due Date: %02d%02d%04d",s1->specs_info->ptp_info.getDueDate().month+1,s1->specs_info->ptp_info.getDueDate().day,s1->specs_info->ptp_info.getDueDate().year);
    fprintf(fp,"\n %s \n",ptp_review_print_info.c_str());
    fprintf(fp,"\n link for PTP: %s",s1->specs_info->ptp_info.getLink().c_str());
    fprintf( fp,"\n tds status: %s",tds_status.c_str());
    fprintf(fp,"\n %s \n",tds_review_print_info.c_str());
    fprintf(fp,"\n pdr status: %s",pdr_status.c_str());
    fprintf( fp,"\n fs Signoff Due Date: %02d%02d%04d",s1->specs_info->pdr_info.getDueDate().month+1,s1->specs_info->pdr_info.getDueDate().day,s1->specs_info->pdr_info.getDueDate().year);
    fprintf(fp,"\n %s \n",pdr_review_print_info.c_str());
    fprintf(fp,"\n link for PDR: %s",s1->specs_info->pdr_info.getLink().c_str());

    fclose ( fp );
    return;
}
/************************************************************************************/
extern int ITK_user_main( int argc, char **argv)
{
    int ifail = ITK_ok;
    int stat = ITK_ok;
    int catValue = -1;
    char *debug_arg     = NULL;
    char *category = ITK_ask_cli_argument( "-c=" );
    char *user_id = ITK_ask_cli_argument( "-u=" );
    char *pass_wd = ITK_ask_cli_argument( "-p=" );
    char *grp_id = ITK_ask_cli_argument( "-g=" );
    char *logFile = ITK_ask_cli_argument( "-l=" );
    char *id = ITK_ask_cli_argument( "-id=" );
    tag_t vol_t = NULLTAG, inItemTag = NULLTAG;
    logical privilege_user = false;
    FILE *fp = NULL;

    (void)argv;
    const char *default_file = "PSR_PhaseII";
    filename = ITK_ask_cli_argument( "-o=" );
    if (filename == NULL)
    {
        filename = static_cast<char *>(malloc(strlen(default_file) +1));
        strcpy(filename,default_file);
        printf("** writing to default file '%s'.\n",filename);
    }
    if (ITK_ask_cli_argument("-h") || (argc==1))
    {
        display_usage();
        return ifail;
    }
    debug_arg = ITK_ask_cli_argument( "-debug" );
    while ( debug_arg )
    {
        prog_debug += 1;
        debug_arg -= tc_strlen( "-debug" );
        *debug_arg++ = '/'; // overwrite this Command Line Option so that it is not processed again
        debug_arg = ITK_ask_cli_argument( "-debug" );
    }
    ITK_initialize_text_services (ITK_BATCH_TEXT_MODE);
    ITK_set_journalling (TRUE);

    fprintf(stderr, "\nLogging in...\n");
    if (( ifail = ITK_init_module( user_id, pass_wd, grp_id )) != ITK_ok)
    {
        fprintf(stderr,"\nERROR: Failed to login : Error = %d \n", ifail);
        ITK_exit_module(TRUE);
        return ifail;
    }

    if (logFile == NULL)
    {
        fprintf(stderr,"\nINFO: Log file has not been provided\n");
        fprintf(stderr,"The information will be published on to the standard output\n\n");
        LOGFILE = stderr;
    }
    else
    {
        fp = fopen (logFile, "w");

        if( fp != NULL )
        {
            LOGFILE = fp;
        }
        else
        {
            fprintf(stderr,"\nINFO: Failed to open the log file\n");
            fprintf(stderr,"The information will be published on to the standard output\n\n");
            LOGFILE = stderr;
        }
    }

    PSR_Project *s1 = new PSR_Project();
    if ( (ifail = s1->get_pbp_info(id) ) != ITK_ok)

    {
        printf("ifail = %d",ifail);
        ITK_exit_module(TRUE);
        return ifail;
    }
    else
    {
        display_pbpinfo( s1 );
    }
    if (fp != NULL)
    {
        fclose ( fp );
    }

    ITK_exit_module(TRUE);

    return ifail;
}