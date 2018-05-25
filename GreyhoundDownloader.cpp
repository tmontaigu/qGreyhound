#include <QFutureWatcher>
#include <QEventLoop>
#include <QtConcurrent>
#include <QThreadPool>

#include <DgmOctree.h>

#include <GreyhoundReader.hpp>

#include "GreyhoundDownloader.h"

std::unique_ptr<ccPointCloud> 
download_and_convert_cloud(pdal::Options opts, PDALConverter converter)
{
	std::exception_ptr eptr(nullptr);
	pdal::PointTable table;
	pdal::PointViewSet view_set;
	pdal::GreyhoundReader reader;
	std::unique_ptr<ccPointCloud> cloud(nullptr);

	reader.addOptions(opts);

	reader.prepare(table);
	view_set = reader.execute(table);
	pdal::PointViewPtr view_ptr = *view_set.begin();
	cloud = converter.convert(view_ptr, table.layout());


	return cloud;
}

std::unique_ptr<ccPointCloud> 
download_and_convert_cloud_threaded(pdal::Options opts, PDALConverter converter)
{
	std::unique_ptr<ccPointCloud> cloud(nullptr);
	std::exception_ptr eptr(nullptr);
	auto dl = [&cloud, &eptr](pdal::Options opts, PDALConverter converter) {
		try {
			cloud = download_and_convert_cloud(opts, converter);
		}
		catch (...) {
			eptr = std::current_exception();
		}
	};

	QFutureWatcher<void> downloader;
	QEventLoop loop;
	downloader.setFuture(QtConcurrent::run(dl, opts, converter));
	QObject::connect(&downloader, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
	loop.exec();
	downloader.waitForFinished();

	if (eptr) {
		std::rethrow_exception(eptr);
	}
	return cloud;
}

GreyhoundDownloader::GreyhoundDownloader(pdal::Options opts, uint32_t start_depth, pdal::greyhound::Bounds bounds, PDALConverter converter)
	: m_opts(opts)
	, m_current_depth(start_depth)
	, m_bounds(bounds)
	, m_converter(converter)
{
}

class DlWorker : public QRunnable
{
public:
	DlWorker(std::function<void(BoundsDepth)> f);
	~DlWorker();
	void run() override;

	BoundsDepth m;
private:
	std::function<void(BoundsDepth)> m_f;

};

DlWorker::DlWorker(std::function<void(BoundsDepth)> f)
	: QRunnable()
	, m_f(f)
{
}

DlWorker::~DlWorker()
{
}

void DlWorker::run()
{
	m_f(m);
}

void
GreyhoundDownloader::download_to(ccPointCloud *cloud, DOWNLOAD_METHOD method, std::function<void()> cb)
{
	std::queue<BoundsDepth> qin;
	std::queue<BoundsDepth> qout;
	
	std::mutex mu_qout;

	using mutex_locker = std::lock_guard<std::mutex>;


	QThreadPool pool;
	pool.setMaxThreadCount(8);
	

	auto f = [&qout, &mu_qout, this](BoundsDepth m) {
		pdal::Options opts(m_opts);
		opts.add("depth_begin", m.depth);
		opts.add("depth_end", m.depth + 1);
		opts.add("bounds", m.b.toJson());
		try {
			std::unique_ptr<ccPointCloud> downloaded_cloud(download_and_convert_cloud(opts, m_converter));
			m.cloud = downloaded_cloud.release();
		}
		catch (const std::exception& e) {
			ccLog::Print(QString("[qGreyhound] %1").arg(e.what()));
			m.cloud = nullptr;
		}

		mutex_locker lk(mu_qout);
		qout.push(m);
	};

	qin.emplace(m_bounds, static_cast<int>(m_current_depth));
	while (!qin.empty() || !qout.empty() || pool.activeThreadCount() != 0)
	{
		if (!qin.empty())
		{
			BoundsDepth m = qin.front();
			qin.pop();
			DlWorker *w = new DlWorker(f);
			w->setAutoDelete(true);
			w->m = m;
			pool.start(w);
		}
		BoundsDepth m;
		{
			mutex_locker lk(mu_qout);
			if (qout.empty()) {
				continue;
			}
			m = qout.front();
			qout.pop();
		}
		if (m.cloud && m.cloud->size() && cloud)
		{
			if (cloud->hasDisplayedScalarField() || cloud->colorsShown()) {
				m.cloud->showSF(false);
				m.cloud->showColors(false);
			}

			cloud->append(m.cloud, cloud->size());
			//Ideally we would like to refresh soon after appending
			//but we can't because the main thread also refreshes the 
			//display from time to time/ on some user action causing crashes?
			//cloud->prepareDisplayForRefresh();
			//cloud->refreshDisplay();

			if (m.depth + 1 <= CCLib::DgmOctree::MAX_OCTREE_LEVEL)
			{
				switch (method)
				{
				case GreyhoundDownloader::DEPTH_BY_DEPTH:
					qin.emplace(m.b, m.depth + 1);
					break;
				case GreyhoundDownloader::QUADTREE:
					qin.emplace(m.b.getSe(), m.depth + 1);
					qin.emplace(m.b.getSw(), m.depth + 1);
					qin.emplace(m.b.getNe(), m.depth + 1);
					qin.emplace(m.b.getNw(), m.depth + 1);
					break;
				case GreyhoundDownloader::OCTREE:
					break;
				default:
					break;
				}
			}
		}
	}
	pool.waitForDone();
}